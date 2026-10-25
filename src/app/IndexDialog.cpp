// SPDX-FileCopyrightText: 2007-2026 Petr Vanek
// SPDX-License-Identifier: GPL-3.0-or-later

#include "IndexDialog.h"

#include "Icons.h"
#include "Session.h"
#include "Settings.h"
#include "catalog/Details.h"
#include "sql/Keywords.h"

#include "ui_IndexDialog.h"

#include <QComboBox>
#include <QHeaderView>
#include <QPushButton>

namespace sqliteman {

IndexDialog::IndexDialog(Session *session, const QString &schema, const QString &table,
                         QWidget *parent)
    : QDialog(parent), m_ui(std::make_unique<Ui::IndexDialog>()), m_session(session),
      m_schema(schema), m_tableName(table)
{
    m_ui->setupUi(this);
    m_ui->buttons->button(QDialogButtonBox::Ok)->setText(tr("C&reate"));
    m_ui->table->setText(schema + QLatin1Char('.') + table);
    m_ui->sql->setFont(Settings::instance().editorFont());

    QTableWidget *columns = m_ui->columns;
    columns->setColumnCount(ColumnCount);
    columns->setHorizontalHeaderLabels({tr("Use"), tr("Column"), tr("Order"), tr("Collate")});
    columns->horizontalHeader()->setSectionResizeMode(Use, QHeaderView::ResizeToContents);
    columns->horizontalHeader()->setSectionResizeMode(Name, QHeaderView::Stretch);
    const catalog::SnapshotPtr snapshot = session ? session->snapshot() : nullptr;
    if (const catalog::SchemaObject *object = snapshot ? snapshot->find(schema, table) : nullptr) {
        for (const catalog::ColumnInfo &c : object->columns) {
            const int row = columns->rowCount();
            columns->insertRow(row);
            auto *use = new QTableWidgetItem;
            use->setFlags(Qt::ItemIsEnabled | Qt::ItemIsUserCheckable | Qt::ItemIsSelectable);
            use->setCheckState(Qt::Unchecked);
            columns->setItem(row, Use, use);
            auto *name = new QTableWidgetItem(c.name);
            name->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
            if (c.primaryKey > 0)
                name->setIcon(Icons::primaryKey());
            name->setToolTip(c.type);
            columns->setItem(row, Name, name);
            auto *order = new QComboBox(columns);
            order->addItems({QStringLiteral("ASC"), QStringLiteral("DESC")});
            connect(order, &QComboBox::currentIndexChanged, this, &IndexDialog::updatePreview);
            columns->setCellWidget(row, Order, order);
            auto *collate = new QComboBox(columns);
            collate->setEditable(true);
            collate->addItem(QString());
            collate->addItems(snapshot->collations);
            connect(collate, &QComboBox::currentTextChanged, this, &IndexDialog::updatePreview);
            columns->setCellWidget(row, Collate, collate);
        }
    }
    m_ui->name->setText(table + QLatin1String("_idx"));

    connect(columns, &QTableWidget::itemChanged, this, &IndexDialog::updatePreview);
    connect(m_ui->name, &QLineEdit::textChanged, this, &IndexDialog::updatePreview);
    connect(m_ui->where, &QLineEdit::textChanged, this, &IndexDialog::updatePreview);
    connect(m_ui->unique, &QCheckBox::toggled, this, &IndexDialog::updatePreview);
    connect(m_ui->ifNotExists, &QCheckBox::toggled, this, &IndexDialog::updatePreview);
    connect(m_ui->moveUp, &QPushButton::clicked, this, [this] { move(-1); });
    connect(m_ui->moveDown, &QPushButton::clicked, this, [this] { move(1); });
    connect(m_ui->buttons, &QDialogButtonBox::accepted, this, [this] {
        apply([this](bool ok) {
            if (ok)
                accept();
        });
    });
    connect(m_ui->buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    updatePreview();
}

IndexDialog::~IndexDialog() = default;

void IndexDialog::setName(const QString &name)
{
    m_ui->name->setText(name);
}

bool IndexDialog::useColumn(const QString &column, bool descending)
{
    for (int row = 0; row < m_ui->columns->rowCount(); ++row) {
        if (m_ui->columns->item(row, Name)->text() != column)
            continue;
        m_ui->columns->item(row, Use)->setCheckState(Qt::Checked);
        qobject_cast<QComboBox *>(m_ui->columns->cellWidget(row, Order))
            ->setCurrentIndex(descending ? 1 : 0);
        return true;
    }
    return false;
}

QString IndexDialog::statement() const
{
    QStringList used;
    for (int row = 0; row < m_ui->columns->rowCount(); ++row) {
        if (m_ui->columns->item(row, Use)->checkState() != Qt::Checked)
            continue;
        QString column = sql::identifier(m_ui->columns->item(row, Name)->text());
        const QString collate = qobject_cast<QComboBox *>(m_ui->columns->cellWidget(row, Collate))
                                    ->currentText()
                                    .trimmed();
        if (!collate.isEmpty())
            column += QLatin1String(" COLLATE ") + collate;
        if (qobject_cast<QComboBox *>(m_ui->columns->cellWidget(row, Order))->currentIndex() == 1)
            column += QLatin1String(" DESC");
        used << column;
    }
    if (used.isEmpty() || m_ui->name->text().trimmed().isEmpty())
        return {};
    return catalog::createIndexStatement(m_schema, m_ui->name->text().trimmed(), m_tableName, used,
                                         m_ui->unique->isChecked(), m_ui->ifNotExists->isChecked(),
                                         m_ui->where->text());
}

QString IndexDialog::statusText() const
{
    return m_ui->status->text();
}

void IndexDialog::updatePreview()
{
    const QString sql = statement();
    m_ui->sql->setPlainText(sql);
    m_ui->buttons->button(QDialogButtonBox::Ok)->setEnabled(!sql.isEmpty());
    if (sql.isEmpty())
        m_ui->status->setText(tr("Name the index and tick the columns it is on."));
    else
        m_ui->status->clear();
}

void IndexDialog::move(int by)
{
    QTableWidget *columns = m_ui->columns;
    const int from = columns->currentRow();
    const int to = from + by;
    if (from < 0 || to < 0 || to >= columns->rowCount())
        return;
    // The widgets in the cells do not move with takeItem(): their state does.
    for (const int c : {int(Use), int(Name)}) {
        QTableWidgetItem *a = columns->takeItem(from, c);
        QTableWidgetItem *b = columns->takeItem(to, c);
        columns->setItem(from, c, b);
        columns->setItem(to, c, a);
    }
    for (const int c : {int(Order), int(Collate)}) {
        auto *a = qobject_cast<QComboBox *>(columns->cellWidget(from, c));
        auto *b = qobject_cast<QComboBox *>(columns->cellWidget(to, c));
        const QString text = a->currentText();
        a->setCurrentText(b->currentText());
        b->setCurrentText(text);
    }
    columns->setCurrentCell(to, Name);
    updatePreview();
}

void IndexDialog::apply(std::function<void(bool ok)> done)
{
    const QString sql = statement();
    if (sql.isEmpty() || !m_session || !m_session->runner()) {
        if (done)
            done(false);
        return;
    }
    m_session->runner()->run(sql.toUtf8(), this, [this, done](const sqlite::QueryOutcome &o) {
        m_ui->status->setText(o.ok() ? tr("Index created.") : o.error);
        if (done)
            done(o.ok());
    });
}

} // namespace sqliteman
