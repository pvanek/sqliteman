// SPDX-FileCopyrightText: 2007-2026 Petr Vanek
// SPDX-License-Identifier: GPL-3.0-or-later

#include "TableDialog.h"

#include "Session.h"
#include "SqlEditor.h"
#include "sql/Keywords.h"

#include "ui_TableDialog.h"

#include <QComboBox>
#include <QHeaderView>
#include <QInputDialog>
#include <QMessageBox>
#include <QPushButton>
#include <QStyledItemDelegate>

#include <sqlite3.h>

namespace sqliteman {

using catalog::ColumnDef;
using catalog::TableDef;

namespace {

// A type is typed, or picked from the names SQLite's documentation uses.
class TypeDelegate : public QStyledItemDelegate
{
public:
    using QStyledItemDelegate::QStyledItemDelegate;

    QWidget *createEditor(QWidget *parent, const QStyleOptionViewItem &,
                          const QModelIndex &) const override
    {
        auto *box = new QComboBox(parent);
        box->setEditable(true);
        box->addItems(sql::typeNames());
        return box;
    }
    void setEditorData(QWidget *editor, const QModelIndex &index) const override
    {
        static_cast<QComboBox *>(editor)->setCurrentText(index.data().toString());
    }
    void setModelData(QWidget *editor, QAbstractItemModel *model,
                      const QModelIndex &index) const override
    {
        model->setData(index, static_cast<QComboBox *>(editor)->currentText().trimmed());
    }
};

QTableWidgetItem *checkItem(bool checked)
{
    auto *item = new QTableWidgetItem;
    item->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsUserCheckable);
    item->setCheckState(checked ? Qt::Checked : Qt::Unchecked);
    return item;
}

} // namespace

TableDialog::TableDialog(Session *session, const QString &schema, QWidget *parent)
    : QDialog(parent), m_ui(std::make_unique<Ui::TableDialog>()), m_session(session)
{
    setUp(schema);
    setWindowTitle(tr("Create Table"));
    m_ui->buttons->button(QDialogButtonBox::Ok)->setText(tr("C&reate"));
    ColumnDef id;
    id.name = QStringLiteral("id");
    id.type = QStringLiteral("INTEGER");
    id.primaryKey = true;
    addColumn(id);
    updatePreview();
    m_ui->name->setFocus();
}

TableDialog::TableDialog(Session *session, const QString &schema, const QString &table,
                         QWidget *parent)
    : QDialog(parent), m_ui(std::make_unique<Ui::TableDialog>()), m_session(session)
{
    setUp(schema);
    setWindowTitle(tr("Alter Table %1").arg(table));
    m_ui->buttons->button(QDialogButtonBox::Ok)->setText(tr("Alte&r"));
    m_ui->schema->setEnabled(false);
    m_ui->ifNotExists->hide();
    m_ui->handWritten->hide();

    const catalog::SnapshotPtr snapshot = session ? session->snapshot() : nullptr;
    const catalog::SchemaObject *object = snapshot ? snapshot->find(schema, table) : nullptr;
    std::optional<TableDef> parsed;
    if (object && object->kind == catalog::ObjectKind::Table)
        parsed = catalog::parseCreateTable(object->sql);
    if (!parsed) {
        m_valid = false;
        QMessageBox::warning(
            parent, tr("Alter Table"),
            tr("%1 cannot be changed here: it is no table made with columns, such as "
               "a virtual table, or its CREATE TABLE could not be read.")
                .arg(table));
        return;
    }
    parsed->schema = schema;
    for (ColumnDef &c : parsed->columns)
        c.originalName = c.name;
    m_before = parsed;
    setDefinition(*parsed);
    m_ui->sql->setReadOnly(true);
}

TableDialog::~TableDialog() = default;

void TableDialog::setUp(const QString &schema)
{
    m_ui->setupUi(this);
    const catalog::SnapshotPtr snapshot = m_session ? m_session->snapshot() : nullptr;
    if (snapshot) {
        for (const catalog::DatabaseInfo &d : snapshot->databases) {
            if (!d.readOnly)
                m_ui->schema->addItem(d.name);
        }
    }
    if (m_ui->schema->findText(schema) < 0)
        m_ui->schema->addItem(schema.isEmpty() ? QStringLiteral("main") : schema);
    m_ui->schema->setCurrentText(schema.isEmpty() ? QStringLiteral("main") : schema);

    QTableWidget *columns = m_ui->columns;
    columns->setColumnCount(ColumnCount);
    columns->setHorizontalHeaderLabels({tr("Name"), tr("Type"), tr("Primary key"),
                                        tr("Autoincrement"), tr("Not null"), tr("Unique"),
                                        tr("Default"), tr("Check"), tr("Collate"), tr("Other")});
    columns->horizontalHeaderItem(Autoincrement)
        ->setToolTip(tr("Never reuse the key of a deleted row; an INTEGER primary key only"));
    columns->horizontalHeaderItem(Default)->setToolTip(
        tr("A number, text, or an expression in parentheses"));
    columns->horizontalHeaderItem(Extra)->setToolTip(
        tr("Whatever else the column says, as SQL: REFERENCES, GENERATED AS …"));
    columns->setItemDelegateForColumn(Type, new TypeDelegate(columns));
    QHeaderView *header = columns->horizontalHeader();
    header->setSectionResizeMode(QHeaderView::Interactive);
    header->setStretchLastSection(true);
    header->resizeSection(Name, 160);
    header->resizeSection(Type, 120);
    for (const int c : {int(PrimaryKey), int(Autoincrement), int(NotNull), int(Unique)})
        header->setSectionResizeMode(c, QHeaderView::ResizeToContents);
    m_ui->constraints->setEditTriggers(QAbstractItemView::DoubleClicked
                                       | QAbstractItemView::EditKeyPressed);

    connect(m_ui->addColumn, &QPushButton::clicked, this, [this] {
        const int row = addColumn();
        m_ui->columns->setCurrentCell(row, Name);
        m_ui->columns->editItem(m_ui->columns->item(row, Name));
    });
    connect(m_ui->removeColumn, &QPushButton::clicked, this, [this] {
        QList<int> rows = selectedRows();
        std::sort(rows.begin(), rows.end(), std::greater<>());
        for (const int row : rows)
            m_ui->columns->removeRow(row);
        updatePreview();
    });
    connect(m_ui->moveUp, &QPushButton::clicked, this, [this] { moveColumn(-1); });
    connect(m_ui->moveDown, &QPushButton::clicked, this, [this] { moveColumn(1); });
    connect(m_ui->addUnique, &QPushButton::clicked, this,
            [this] { addColumnsConstraint(QStringLiteral("UNIQUE"), selectedRows()); });
    connect(m_ui->addPrimaryKey, &QPushButton::clicked, this,
            [this] { addColumnsConstraint(QStringLiteral("PRIMARY KEY"), selectedRows()); });
    connect(m_ui->addOther, &QPushButton::clicked, this, [this] {
        bool ok = false;
        const QString text = QInputDialog::getMultiLineText(
            this, tr("Add Constraint"),
            tr("The constraint as SQL, such as CHECK (a < b) or FOREIGN KEY (a) REFERENCES t (b):"),
            QStringLiteral("CHECK ()"), &ok);
        if (ok && !text.trimmed().isEmpty()) {
            auto *item = new QListWidgetItem(text.trimmed(), m_ui->constraints);
            item->setFlags(item->flags() | Qt::ItemIsEditable);
            updatePreview();
        }
    });
    connect(m_ui->removeConstraint, &QPushButton::clicked, this, [this] {
        qDeleteAll(m_ui->constraints->selectedItems());
        updatePreview();
    });
    connect(m_ui->constraints, &QListWidget::itemChanged, this, &TableDialog::updatePreview);
    connect(m_ui->columns, &QTableWidget::itemChanged, this, &TableDialog::updatePreview);
    connect(m_ui->columns, &QTableWidget::itemSelectionChanged, this, &TableDialog::updateEnabled);
    connect(m_ui->name, &QLineEdit::textChanged, this, &TableDialog::updatePreview);
    connect(m_ui->schema, &QComboBox::currentTextChanged, this, &TableDialog::updatePreview);
    for (QCheckBox *box : {m_ui->ifNotExists, m_ui->withoutRowid, m_ui->strict})
        connect(box, &QCheckBox::toggled, this, &TableDialog::updatePreview);
    connect(m_ui->handWritten, &QCheckBox::toggled, this, [this](bool on) {
        m_ui->sql->setReadOnly(!on);
        m_ui->columnsPage->setEnabled(!on);
        m_ui->constraintsPage->setEnabled(!on);
        m_ui->name->setEnabled(!on);
        if (!on)
            updatePreview();
        updateEnabled();
    });
    connect(m_ui->sql, &QsciScintilla::textChanged, this, &TableDialog::updateEnabled);
    m_ui->sql->setReadOnly(true);
    connect(m_ui->buttons, &QDialogButtonBox::accepted, this, [this] {
        apply([this](bool ok) {
            if (ok)
                accept();
        });
    });
    connect(m_ui->buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
}

int TableDialog::addColumn(const ColumnDef &c)
{
    QTableWidget *columns = m_ui->columns;
    const QSignalBlocker blocker(columns);
    const int row = columns->rowCount();
    columns->insertRow(row);
    auto *name = new QTableWidgetItem(c.name);
    name->setData(Qt::UserRole, c.originalName);
    if (!c.originalName.isEmpty())
        name->setToolTip(tr("Now %1 in the table").arg(c.originalName));
    columns->setItem(row, Name, name);
    columns->setItem(row, Type, new QTableWidgetItem(c.type));
    columns->setItem(row, PrimaryKey, checkItem(c.primaryKey));
    columns->setItem(row, Autoincrement, checkItem(c.autoincrement));
    columns->setItem(row, NotNull, checkItem(c.notNull));
    columns->setItem(row, Unique, checkItem(c.unique));
    columns->setItem(row, Default, new QTableWidgetItem(c.defaultValue));
    columns->setItem(row, Check, new QTableWidgetItem(c.check));
    columns->setItem(row, Collate, new QTableWidgetItem(c.collate));
    columns->setItem(row, Extra, new QTableWidgetItem(c.extra));
    updatePreview();
    return row;
}

void TableDialog::setDefinition(const TableDef &table)
{
    m_ui->schema->setCurrentText(table.schema.isEmpty() ? QStringLiteral("main") : table.schema);
    m_ui->name->setText(table.name);
    m_ui->ifNotExists->setChecked(table.ifNotExists);
    m_ui->withoutRowid->setChecked(table.withoutRowid);
    m_ui->strict->setChecked(table.strict);
    m_ui->columns->setRowCount(0);
    for (const ColumnDef &c : table.columns)
        addColumn(c);
    m_ui->constraints->clear();
    for (const QString &constraint : table.constraints) {
        auto *item = new QListWidgetItem(constraint, m_ui->constraints);
        item->setFlags(item->flags() | Qt::ItemIsEditable);
    }
    updatePreview();
}

TableDef TableDialog::definition() const
{
    TableDef t;
    t.schema = m_ui->schema->currentText();
    t.name = m_ui->name->text().trimmed();
    t.ifNotExists = m_ui->ifNotExists->isChecked() && !isAltering();
    t.withoutRowid = m_ui->withoutRowid->isChecked();
    t.strict = m_ui->strict->isChecked();
    const QTableWidget *columns = m_ui->columns;
    auto text = [columns](int row, int column) {
        const QTableWidgetItem *item = columns->item(row, column);
        return item ? item->text().trimmed() : QString();
    };
    auto checked = [columns](int row, int column) {
        const QTableWidgetItem *item = columns->item(row, column);
        return item && item->checkState() == Qt::Checked;
    };
    for (int row = 0; row < columns->rowCount(); ++row) {
        ColumnDef c;
        c.name = text(row, Name);
        if (c.name.isEmpty())
            continue;
        c.originalName = columns->item(row, Name)->data(Qt::UserRole).toString();
        c.type = text(row, Type);
        c.primaryKey = checked(row, PrimaryKey);
        c.autoincrement = c.primaryKey && checked(row, Autoincrement);
        c.notNull = checked(row, NotNull);
        c.unique = checked(row, Unique);
        // Kept as written for a column as it was; typed anew, made into SQL.
        const QString def = text(row, Default);
        const ColumnDef *old = nullptr;
        if (m_before) {
            for (const ColumnDef &b : m_before->columns)
                if (b.name == c.originalName)
                    old = &b;
        }
        c.defaultValue = old && old->defaultValue == def ? def : catalog::defaultExpression(def);
        c.check = text(row, Check);
        c.collate = text(row, Collate);
        c.extra = text(row, Extra);
        t.columns.push_back(c);
    }
    for (int i = 0; i < m_ui->constraints->count(); ++i) {
        const QString constraint = m_ui->constraints->item(i)->text().trimmed();
        if (!constraint.isEmpty())
            t.constraints << constraint;
    }
    return t;
}

catalog::AlterRequest TableDialog::request() const
{
    catalog::AlterRequest r;
    r.before = *m_before;
    r.after = definition();
    r.after.schema = r.before.schema;
    return r;
}

QString TableDialog::statement() const
{
    if (!isAltering()) {
        if (m_ui->handWritten->isChecked())
            return m_ui->sql->text();
        return catalog::createTableStatement(definition()) + QLatin1Char(';');
    }
    const catalog::AlterRequest r = request();
    // The indexes and triggers as the plan finds them after the renames.
    QStringList indexes, triggers;
    if (const catalog::SnapshotPtr snapshot = m_session ? m_session->snapshot() : nullptr) {
        const auto renamed = [&r](QString sql) {
            for (const ColumnDef &c : r.after.columns) {
                if (!c.originalName.isEmpty() && c.originalName != c.name)
                    sql = catalog::renameIdentifier(sql, c.originalName, c.name);
            }
            return sql;
        };
        for (const auto *i :
             snapshot->belongingTo(r.before.schema, r.before.name, catalog::ObjectKind::Index))
            if (!i->sql.isEmpty())
                indexes << renamed(i->sql);
        for (const auto *t :
             snapshot->belongingTo(r.before.schema, r.before.name, catalog::ObjectKind::Trigger))
            triggers << t->sql;
    }
    const catalog::AlterPlan plan = catalog::planAlterTable(r, indexes, triggers);
    if (plan.isEmpty())
        return tr("-- Nothing has changed.");
    return plan.preview();
}

QString TableDialog::logText() const
{
    return m_ui->log->toPlainText();
}

void TableDialog::updatePreview()
{
    if (!m_ui->handWritten->isChecked()) {
        const QSignalBlocker blocker(m_ui->sql);
        m_ui->sql->setText(statement());
    }
    updateEnabled();
}

void TableDialog::updateEnabled()
{
    const TableDef t = definition();
    const bool hand = m_ui->handWritten->isChecked();
    QString problem;
    if (!hand && t.name.isEmpty())
        problem = tr("Name the table.");
    else if (!hand && t.columns.empty())
        problem = tr("Give the table a column.");
    else if (hand && m_ui->sql->text().trimmed().isEmpty())
        problem = tr("Write the CREATE TABLE statement.");
    m_ui->buttons->button(QDialogButtonBox::Ok)->setEnabled(problem.isEmpty() && !m_running);
    m_ui->buttons->button(QDialogButtonBox::Ok)->setToolTip(problem);
    const bool selected = !selectedRows().isEmpty();
    m_ui->removeColumn->setEnabled(selected);
    m_ui->moveUp->setEnabled(selected);
    m_ui->moveDown->setEnabled(selected);
}

QList<int> TableDialog::selectedRows() const
{
    QList<int> rows;
    for (const QModelIndex &index : m_ui->columns->selectionModel()->selectedRows())
        rows << index.row();
    std::sort(rows.begin(), rows.end());
    return rows;
}

void TableDialog::moveColumn(int by)
{
    const QList<int> rows = selectedRows();
    if (rows.size() != 1)
        return;
    const int from = rows.front();
    const int to = from + by;
    QTableWidget *columns = m_ui->columns;
    if (to < 0 || to >= columns->rowCount())
        return;
    {
        const QSignalBlocker blocker(columns);
        for (int c = 0; c < ColumnCount; ++c) {
            QTableWidgetItem *a = columns->takeItem(from, c);
            QTableWidgetItem *b = columns->takeItem(to, c);
            columns->setItem(from, c, b);
            columns->setItem(to, c, a);
        }
    }
    columns->selectRow(to);
    updatePreview();
}

void TableDialog::addColumnsConstraint(const QString &keyword, const QList<int> &rows)
{
    QStringList names;
    for (const int row : rows) {
        if (const QTableWidgetItem *item = m_ui->columns->item(row, Name);
            item && !item->text().isEmpty())
            names << item->text().trimmed();
    }
    if (names.isEmpty()) {
        bool ok = false;
        const QString typed = QInputDialog::getText(
            this, tr("Add %1").arg(keyword),
            tr("Columns, separated by commas (or select them on the Columns tab first):"),
            QLineEdit::Normal, QString(), &ok);
        if (!ok)
            return;
        for (const QString &name : typed.split(QLatin1Char(','), Qt::SkipEmptyParts))
            names << name.trimmed();
        if (names.isEmpty())
            return;
    }
    auto *item = new QListWidgetItem(catalog::columnsConstraint(keyword, names), m_ui->constraints);
    item->setFlags(item->flags() | Qt::ItemIsEditable);
    m_ui->tabs->setCurrentWidget(m_ui->constraintsPage);
    updatePreview();
}

void TableDialog::log(const QString &text, bool error)
{
    m_ui->log->appendPlainText(error ? tr("Error: %1").arg(text) : text);
}

void TableDialog::apply(std::function<void(bool ok)> done)
{
    if (m_running || !m_session || !m_session->runner()) {
        if (done)
            done(false);
        return;
    }
    m_running = true;
    updateEnabled();
    auto finish = [this, done](bool ok) {
        m_running = false;
        updateEnabled();
        if (done)
            done(ok);
    };

    if (!isAltering()) {
        const QString sql = statement();
        m_session->runner()->run(sql.toUtf8(), this, [this, finish](const sqlite::QueryOutcome &o) {
            if (o.ok())
                log(tr("Table created."));
            else
                log(o.error, true);
            finish(o.ok());
        });
        return;
    }

    const catalog::AlterRequest r = request();
    m_session->runner()->perform([r](sqlite3 *db) { return catalog::alterTable(db, r); }, this,
                                 [this, finish](const sqlite::QueryOutcome &o) {
                                     for (const QString &line : o.task.value.toStringList())
                                         log(line);
                                     if (o.ok())
                                         log(tr("Table altered."));
                                     else
                                         log(o.error, true);
                                     // A task, not SQL: the connection's own watch for schema
                                     // changes does not see it.
                                     if (o.ok() && m_session)
                                         m_session->reloadSnapshot();
                                     finish(o.ok());
                                 });
}

} // namespace sqliteman
