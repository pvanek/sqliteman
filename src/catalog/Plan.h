// SPDX-FileCopyrightText: 2026 Petr Vanek
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "sqlite/RowStore.h"

#include <QByteArray>
#include <QString>

#include <optional>
#include <vector>

namespace sqliteman::catalog {

// What EXPLAIN QUERY PLAN tells about how SQLite runs a query: a tree of
// steps, each a line of text such as "SEARCH t USING INDEX t_a (a=?)".
struct PlanStep
{
    enum class Kind {
        Scan, // Every row of a table: worth a look on a big one.
        Search, // Through an index or the rowid.
        TempBTree, // A sort or DISTINCT that needs a table of its own.
        Other, // Subqueries, compound parts, CTEs and the like.
    };

    int id = 0;
    QString detail;
    Kind kind = Kind::Other;
    QString object; // The table or index the step reads, if any.
    std::vector<int> children; // Indexes into Plan::steps.
};

struct Plan
{
    std::vector<PlanStep> steps;
    std::vector<int> roots; // Indexes into steps, in order.

    bool isEmpty() const { return steps.empty(); }
};

// The statement prefixed so that SQLite describes it instead of running it.
QByteArray explainStatement(const QByteArray &statement);

// From the rows EXPLAIN QUERY PLAN gives: id, parent, notused, detail.
// Nullopt if the rows do not look like a plan.
std::optional<Plan> parsePlan(const sqlite::RowStore &rows);

// SQLite names a table in a plan by the alias the statement gives it, as
// "SCAN c" for "FROM Customer c"; this puts the table's own name in each
// step's object instead.
void nameTables(Plan &plan, const QByteArray &statement);

// The plan as indented text, as the sqlite3 shell prints it.
QString planText(const Plan &plan);

} // namespace sqliteman::catalog
