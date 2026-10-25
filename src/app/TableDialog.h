// SPDX-FileCopyrightText: 2007-2026 Petr Vanek
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "catalog/AlterTable.h"
#include "catalog/TableDefinition.h"

#include <QDialog>
#include <QPointer>

#include <memory>
#include <optional>

namespace Ui {
class TableDialog;
}

namespace sqliteman {

class Session;

// Makes a table, or changes one: its name, its columns with their types
// and constraints, and the constraints over several columns. What it will
// run is on the SQL tab before it runs; a change SQLite's ALTER TABLE
// cannot make rebuilds the table, keeping its rows, indexes and triggers.
class TableDialog : public QDialog
{
    Q_OBJECT

public:
    enum Column {
        Name,
        Type,
        PrimaryKey,
        Autoincrement,
        NotNull,
        Unique,
        Default,
        Check,
        Collate,
        Extra,
        ColumnCount
    };

    // A new table in schema.
    TableDialog(Session *session, const QString &schema, QWidget *parent = nullptr);
    // Changes schema.table.
    TableDialog(Session *session, const QString &schema, const QString &table,
                QWidget *parent = nullptr);
    ~TableDialog() override;

    // False when the table to change could not be read, as for a virtual
    // table; the dialog has said why.
    bool isValid() const { return m_valid; }
    bool isAltering() const { return m_before.has_value(); }

    // What the tabs describe now.
    catalog::TableDef definition() const;
    void setDefinition(const catalog::TableDef &table);
    // What OK runs: CREATE TABLE, or the alteration's plan.
    QString statement() const;
    QString logText() const;

    // Adds a column at the end; returns its row.
    int addColumn(const catalog::ColumnDef &column = {});
    // A UNIQUE or PRIMARY KEY constraint on the columns of the rows given.
    void addColumnsConstraint(const QString &keyword, const QList<int> &rows);

    // Runs it; done says how it went. Tests call it instead of pressing OK.
    void apply(std::function<void(bool ok)> done = {});

private:
    void setUp(const QString &schema);
    void updatePreview();
    void updateEnabled();
    void moveColumn(int by);
    QList<int> selectedRows() const;
    void log(const QString &text, bool error = false);
    catalog::AlterRequest request() const;

    std::unique_ptr<Ui::TableDialog> m_ui; // Layout in TableDialog.ui.
    QPointer<Session> m_session;
    std::optional<catalog::TableDef> m_before; // The table as it is, when altering.
    bool m_valid = true;
    bool m_running = false;
};

} // namespace sqliteman
