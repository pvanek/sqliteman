// SPDX-FileCopyrightText: 2026 Petr Vanek
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "catalog/Snapshot.h"
#include "sqlite/Connection.h"
#include "sqlite/RowStore.h"
#include "sqlite/Value.h"

#include <QByteArray>
#include <QString>

#include <vector>

namespace sqliteman::catalog {

// The table a result's rows can be written back to.
struct EditTarget
{
    QString schema;
    QString table;
    // Per result column: its name in the table, or empty when it is not
    // one of the table's columns (computed, or from elsewhere).
    std::vector<QString> columns;
    std::vector<bool> readOnly; // Generated columns, and the rowid.
    std::vector<Affinity> affinity; // What a value typed into the column turns into.
    std::vector<int> key; // Result columns that pick out a row, in key order.
    bool keyIsRowid = false; // key is the one column holding the rowid.
    QString reason; // Why the result is not editable, if it is not.

    bool editable() const { return !key.empty(); }
    bool writable(int column) const
    {
        return editable() && column < int(columns.size()) && !columns[std::size_t(column)].isEmpty()
            && !readOnly[std::size_t(column)];
    }
};

// Where the result's rows come from, by SQLite's column metadata: the one
// table every column read from a table is in. Rows of a table with a rowid
// are found by it when the result has it, else by the primary key.
EditTarget editTarget(const sqlite::RowStore &rows, const Snapshot &snapshot);

// What a value typed into a cell is stored as: a number in a numeric
// column when it reads as one, text otherwise, as SQLite's affinity would
// convert it anyway.
sqlite::Value typedValue(const QString &text, Affinity affinity);

// A change to one row of an editable result.
struct RowEdit
{
    enum class Kind { Update, Insert, Delete };

    Kind kind = Kind::Update;
    std::vector<sqlite::Value> key; // The row's key before the change (Update, Delete).
    std::vector<std::pair<int, sqlite::Value>> values; // Result column and new value.
};

struct DmlStatement
{
    QByteArray sql; // With ?NNN parameters.
    std::vector<sqlite::Value> parameters;

    // The statement with the values written in, to show what will run;
    // long values cut short.
    QString preview() const;
};

// One statement per edit, each meant to change exactly one row.
std::vector<DmlStatement> dmlStatements(const EditTarget &target,
                                        const std::vector<RowEdit> &edits);

// Runs the statements on the connection's thread, all of them or none: in
// a savepoint, which nests in a transaction already open. A statement that
// does not change exactly one row undoes them all. The outcome's value is
// how many ran.
sqlite::TaskOutcome applyEdits(sqlite3 *db, const std::vector<DmlStatement> &statements);

} // namespace sqliteman::catalog
