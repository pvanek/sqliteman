// SPDX-FileCopyrightText: 2007-2026 Petr Vanek
// SPDX-License-Identifier: GPL-3.0-or-later

#include "QueryBuilderDialog.h"

#include "Settings.h"

#include "ui_QueryBuilderDialog.h"

#include <QComboBox>
#include <QHeaderView>
#include <QLineEdit>
#include <QPushButton>

namespace sqliteman {

using Relation = catalog::QuerySpec::Relation;

namespace {

enum TermColumn { TermField, TermRelation, TermValue };

} // namespace

QueryBuilderDialog::QueryBuilderDialog(const catalog::SnapshotPtr &snapshot, QWidget *parent)
    : QDialog(parent), m_ui(std::make_unique<Ui::QueryBuilderDialog>()), m_snapshot(snapshot)
{
    m_ui->setupUi(this);
    m_ui->sql->setFont(Settings::instance().editorFont());
    m_ui->terms->setColumnCount(3);
    m_ui->terms->setHorizontalHeaderLabels({tr("Column"), tr("Relation"), tr("Value")});
    m_ui->terms->horizontalHeader()->setStretchLastSection(true);
    if (snapshot) {
        for (const catalog::DatabaseInfo &d : snapshot->databases) {
            for (const catalog::SchemaObject &o : d.objects) {
                if ((o.kind == catalog::ObjectKind::Table || o.kind == catalog::ObjectKind::View
                     || o.kind == catalog::ObjectKind::VirtualTable)
                    && !o.system)
                    m_ui->table->addItem(d.name + QLatin1Char('.') + o.name,
                                         QStringList {d.name, o.name});
            }
        }
    }

    connect(m_ui->table, &QComboBox::currentIndexChanged, this, &QueryBuilderDialog::tableChanged);
    connect(m_ui->add, &QPushButton::clicked, this, [this] { moveColumns(true, false); });
    connect(m_ui->addAll, &QPushButton::clicked, this, [this] { moveColumns(true, true); });
    connect(m_ui->remove, &QPushButton::clicked, this, [this] { moveColumns(false, false); });
    connect(m_ui->removeAll, &QPushButton::clicked, this, [this] { moveColumns(false, true); });
    connect(m_ui->available, &QListWidget::itemDoubleClicked, this,
            [this] { moveColumns(true, false); });
    connect(m_ui->selected, &QListWidget::itemDoubleClicked, this,
            [this] { moveColumns(false, false); });
    connect(m_ui->distinct, &QCheckBox::toggled, this, &QueryBuilderDialog::updatePreview);
    connect(m_ui->addTerm, &QPushButton::clicked, this,
            [this] { addTerm(m_columns.value(0), Relation::Equals, QString()); });
    connect(m_ui->removeTerm, &QPushButton::clicked, this, [this] {
        if (m_ui->terms->currentRow() >= 0)
            m_ui->terms->removeRow(m_ui->terms->currentRow());
        updatePreview();
    });
    connect(m_ui->matchAll, &QRadioButton::toggled, this, &QueryBuilderDialog::updatePreview);
    connect(m_ui->order, &QComboBox::currentIndexChanged, this, &QueryBuilderDialog::updatePreview);
    connect(m_ui->descending, &QCheckBox::toggled, this, &QueryBuilderDialog::updatePreview);
    connect(m_ui->limit, &QSpinBox::valueChanged, this, &QueryBuilderDialog::updatePreview);
    connect(m_ui->buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(m_ui->buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    tableChanged();
}

QueryBuilderDialog::~QueryBuilderDialog() = default;

void QueryBuilderDialog::setTable(const QString &schema, const QString &table)
{
    const int index = m_ui->table->findText(schema + QLatin1Char('.') + table);
    if (index >= 0)
        m_ui->table->setCurrentIndex(index);
}

void QueryBuilderDialog::tableChanged()
{
    const QStringList chosen = m_ui->table->currentData().toStringList();
    m_columns.clear();
    if (m_snapshot && chosen.size() == 2) {
        if (const catalog::SchemaObject *o = m_snapshot->find(chosen[0], chosen[1])) {
            for (const catalog::ColumnInfo &c : o->columns)
                m_columns << c.name;
        }
    }
    m_ui->available->clear();
    m_ui->available->addItems(m_columns);
    m_ui->selected->clear();
    m_ui->terms->setRowCount(0);
    const QSignalBlocker blocker(m_ui->order);
    m_ui->order->clear();
    m_ui->order->addItem(QString());
    m_ui->order->addItems(m_columns);
    m_ui->buttons->button(QDialogButtonBox::Ok)->setEnabled(chosen.size() == 2);
    updatePreview();
}

void QueryBuilderDialog::moveColumns(bool toSelected, bool all)
{
    QListWidget *from = toSelected ? m_ui->available : m_ui->selected;
    QListWidget *to = toSelected ? m_ui->selected : m_ui->available;
    QList<QListWidgetItem *> items
        = all ? from->findItems(QStringLiteral("*"), Qt::MatchWildcard) : from->selectedItems();
    for (QListWidgetItem *item : items)
        to->addItem(from->takeItem(from->row(item)));
    // What is left to choose from stays in the table's order.
    if (!toSelected) {
        QStringList left;
        for (int i = 0; i < m_ui->available->count(); ++i)
            left << m_ui->available->item(i)->text();
        m_ui->available->clear();
        for (const QString &c : std::as_const(m_columns))
            if (left.contains(c))
                m_ui->available->addItem(c);
    }
    updatePreview();
}

void QueryBuilderDialog::selectColumns(const QStringList &columns)
{
    for (const QString &c : columns) {
        const auto found = m_ui->available->findItems(c, Qt::MatchExactly);
        for (QListWidgetItem *item : found)
            m_ui->selected->addItem(m_ui->available->takeItem(m_ui->available->row(item)));
    }
    updatePreview();
}

void QueryBuilderDialog::addTerm(const QString &column, Relation relation, const QString &value)
{
    const int row = m_ui->terms->rowCount();
    m_ui->terms->insertRow(row);
    auto *field = new QComboBox(m_ui->terms);
    field->addItems(m_columns);
    field->setCurrentText(column);
    connect(field, &QComboBox::currentIndexChanged, this, &QueryBuilderDialog::updatePreview);
    m_ui->terms->setCellWidget(row, TermField, field);
    auto *relations = new QComboBox(m_ui->terms);
    for (int r = 0; r <= int(Relation::IsNotNull); ++r)
        relations->addItem(catalog::relationName(Relation(r)), r);
    relations->setCurrentIndex(relations->findData(int(relation)));
    m_ui->terms->setCellWidget(row, TermRelation, relations);
    auto *edit = new QLineEdit(value, m_ui->terms);
    connect(edit, &QLineEdit::textChanged, this, &QueryBuilderDialog::updatePreview);
    m_ui->terms->setCellWidget(row, TermValue, edit);
    connect(relations, &QComboBox::currentIndexChanged, this, [this, relations, edit] {
        edit->setEnabled(catalog::takesValue(Relation(relations->currentData().toInt())));
        updatePreview();
    });
    edit->setEnabled(catalog::takesValue(relation));
    updatePreview();
}

catalog::QuerySpec QueryBuilderDialog::spec() const
{
    catalog::QuerySpec spec;
    const QStringList chosen = m_ui->table->currentData().toStringList();
    if (chosen.size() == 2) {
        spec.schema = chosen[0];
        spec.table = chosen[1];
    }
    spec.distinct = m_ui->distinct->isChecked();
    for (int i = 0; i < m_ui->selected->count(); ++i)
        spec.columns << m_ui->selected->item(i)->text();
    for (int row = 0; row < m_ui->terms->rowCount(); ++row) {
        catalog::QuerySpec::Term term;
        term.column
            = qobject_cast<QComboBox *>(m_ui->terms->cellWidget(row, TermField))->currentText();
        term.relation
            = Relation(qobject_cast<QComboBox *>(m_ui->terms->cellWidget(row, TermRelation))
                           ->currentData()
                           .toInt());
        term.value = qobject_cast<QLineEdit *>(m_ui->terms->cellWidget(row, TermValue))->text();
        spec.terms.push_back(term);
    }
    spec.matchAll = m_ui->matchAll->isChecked();
    if (!m_ui->order->currentText().isEmpty())
        spec.order.push_back({m_ui->order->currentText(), m_ui->descending->isChecked()});
    spec.limit = m_ui->limit->value();
    return spec;
}

QString QueryBuilderDialog::statement() const
{
    const catalog::QuerySpec s = spec();
    return s.table.isEmpty() ? QString() : catalog::selectStatement(s) + QLatin1Char(';');
}

void QueryBuilderDialog::updatePreview()
{
    m_ui->sql->setPlainText(statement());
}

} // namespace sqliteman
