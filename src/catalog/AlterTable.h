// SPDX-FileCopyrightText: 2007-2026 Petr Vanek
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "catalog/TableDefinition.h"
#include "sqlite/Connection.h"

#include <QStringList>

struct sqlite3;

namespace sqliteman::catalog {

// A table as it is and as it is to be. after.columns name their column in
// before by originalName; one without is added, and a column of before that
// no column of after names is dropped. after.schema must be before.schema.
struct AlterRequest
{
    TableDef before;
    TableDef after;
};

// How a table gets from before to after. SQLite's ALTER TABLE renames,
// adds and drops columns; for anything else the table is rebuilt the way
// its documentation describes: a new table, the rows copied over, the old
// one dropped and the new one renamed, its indexes and triggers made again.
struct AlterPlan
{
    // First the renames, done by ALTER TABLE, which also fixes the names in
    // indexes, triggers and views.
    QStringList renames;
    // Then either more ALTER TABLE, or the rebuild.
    bool rebuild = false;
    QStringList statements;
    QStringList warnings; // What will be lost, like an index on a dropped column.

    bool isEmpty() const { return renames.isEmpty() && statements.isEmpty(); }
    // Everything in order, with the transaction around it, to show before
    // it runs.
    QString preview() const;
};

// indexSql and triggerSql are the CREATE statements of the table's indexes
// and triggers, with the renames done (see renameColumn()).
AlterPlan planAlterTable(const AlterRequest &request, const QStringList &indexSql,
                         const QStringList &triggerSql);

// Does the whole alteration on the connection's thread, in one transaction
// (a savepoint if one is open already), and undoes all of it if a step
// fails, a foreign key check after a rebuild included. The outcome's value
// is the statements that ran.
sqlite::TaskOutcome alterTable(sqlite3 *db, const AlterRequest &request);

// SQL with every name token old replaced by new, as RENAME COLUMN does it.
QString renameIdentifier(const QString &sql, const QString &from, const QString &to);
// A CREATE INDEX or CREATE TRIGGER as sqlite_schema keeps it, with the
// object's name put in schema; outside main, that is where it has to go.
QString qualifyCreate(const QString &sql, const QString &schema);

} // namespace sqliteman::catalog
