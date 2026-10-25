// SPDX-FileCopyrightText: 2007-2026 Petr Vanek
// SPDX-License-Identifier: GPL-3.0-or-later

#include "PopulatorDialog.h"

#include "Session.h"

#include "ui_PopulatorDialog.h"

#include <QComboBox>
#include <QHeaderView>
#include <QLineEdit>
#include <QPushButton>
#include <QSpinBox>

#include <sqlite3.h>

namespace sqliteman {

namespace populator = catalog::populator;
using populator::Action;

PopulatorDialog::PopulatorDialog(Session *session, const QString &schema, const QString &table,
                                 QWidget *parent)
    : QDialog(parent), m_ui(std::make_unique<Ui::PopulatorDialog>()), m_session(session),
      m_schema(schema), m_tableName(table)
{
    m_ui->setupUi(this);
    m_ui->buttons->button(QDialogButtonBox::Ok)->setText(tr("&Populate"));
    m_ui->table->setText(schema + QLatin1Char('.') + table);

    const catalog::SnapshotPtr snapshot = session ? session->snapshot() : nullptr;
    const catalog::SchemaObject *object = snapshot ? snapshot->find(schema, table) : nullptr;
    m_valid = object && object->kind == catalog::ObjectKind::Table;
    if (m_valid)
        m_columns = populator::columnsOf(*object);

    QTableWidget *columns = m_ui->columns;
    columns->setColumnCount(ColumnCount);
    columns->setHorizontalHeaderLabels(
        {tr("Column"), tr("Type"), tr("Fill with"), tr("Size"), tr("Value")});
    columns->horizontalHeader()->setStretchLastSection(true);
    columns->setRowCount(int(m_columns.size()));
    for (int row = 0; row < int(m_columns.size()); ++row) {
        const populator::Column &c = m_columns[std::size_t(row)];
        auto *name = new QTableWidgetItem(c.name);
        name->setFlags(Qt::ItemIsEnabled);
        columns->setItem(row, NameColumn, name);
        auto *type = new QTableWidgetItem(c.type);
        type->setFlags(Qt::ItemIsEnabled);
        columns->setItem(row, TypeColumn, type);
        auto *action = new QComboBox(columns);
        for (int a = 0; a <= int(Action::Ignore); ++a)
            action->addItem(populator::actionName(Action(a)), a);
        action->setCurrentIndex(action->findData(int(c.action)));
        connect(action, &QComboBox::currentIndexChanged, this, [this, row] {
            updateRow(row);
            updateEnabled();
        });
        columns->setCellWidget(row, ActionColumn, action);
        auto *size = new QSpinBox(columns);
        size->setRange(1, 1000);
        size->setValue(c.size);
        size->setToolTip(tr("Digits of a number, letters of a text"));
        columns->setCellWidget(row, SizeColumn, size);
        auto *value = new QLineEdit(columns);
        value->setToolTip(tr("The prefix, the static value, or the number to count on from"));
        columns->setCellWidget(row, ValueColumn, value);
        updateRow(row);
    }
    columns->resizeColumnsToContents();

    connect(m_ui->buttons, &QDialogButtonBox::accepted, this, [this] { apply(); });
    connect(m_ui->buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    updateEnabled();
}

PopulatorDialog::~PopulatorDialog() = default;

void PopulatorDialog::updateRow(int row)
{
    auto *action = qobject_cast<QComboBox *>(m_ui->columns->cellWidget(row, ActionColumn));
    const auto chosen = populator::Action(action->currentData().toInt());
    m_ui->columns->cellWidget(row, SizeColumn)->setEnabled(populator::takesSize(chosen));
    m_ui->columns->cellWidget(row, ValueColumn)->setEnabled(populator::takesValue(chosen));
}

void PopulatorDialog::updateEnabled()
{
    bool any = false;
    for (const populator::Column &c : columns())
        any = any || c.action != Action::Ignore;
    m_ui->buttons->button(QDialogButtonBox::Ok)->setEnabled(m_valid && any && !m_running);
}

std::vector<populator::Column> PopulatorDialog::columns() const
{
    std::vector<populator::Column> out = m_columns;
    for (int row = 0; row < int(out.size()); ++row) {
        populator::Column &c = out[std::size_t(row)];
        c.action = Action(qobject_cast<QComboBox *>(m_ui->columns->cellWidget(row, ActionColumn))
                              ->currentData()
                              .toInt());
        c.size = qobject_cast<QSpinBox *>(m_ui->columns->cellWidget(row, SizeColumn))->value();
        c.value = qobject_cast<QLineEdit *>(m_ui->columns->cellWidget(row, ValueColumn))->text();
    }
    return out;
}

void PopulatorDialog::setAction(int row, Action action, const QString &value)
{
    auto *box = qobject_cast<QComboBox *>(m_ui->columns->cellWidget(row, ActionColumn));
    box->setCurrentIndex(box->findData(int(action)));
    qobject_cast<QLineEdit *>(m_ui->columns->cellWidget(row, ValueColumn))->setText(value);
}

void PopulatorDialog::setCount(int count)
{
    m_ui->count->setValue(count);
}

QString PopulatorDialog::statusText() const
{
    return m_ui->status->text();
}

void PopulatorDialog::apply(std::function<void(bool ok)> done)
{
    if (!m_valid || m_running || !m_session || !m_session->runner()) {
        if (done)
            done(false);
        return;
    }
    m_running = true;
    updateEnabled();
    m_ui->status->setText(tr("Populating…"));
    const auto chosen = columns();
    const int count = m_ui->count->value();
    const bool skip = m_ui->skipErrors->isChecked();
    m_session->runner()->perform(
        [schema = m_schema, table = m_tableName, chosen, count, skip](sqlite3 *db) {
            return populator::populate(db, schema, table, chosen, count, skip);
        },
        this,
        [this, done](const sqlite::QueryOutcome &o) {
            m_running = false;
            updateEnabled();
            m_ui->status->setText(o.ok() ? tr("Inserted %n row(s).", nullptr, o.task.value.toInt())
                                         : tr("Nothing was inserted: %1").arg(o.error));
            if (done)
                done(o.ok());
        });
}

} // namespace sqliteman
