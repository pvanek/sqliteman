// SPDX-FileCopyrightText: 2026 Petr Vanek
// SPDX-License-Identifier: GPL-3.0-or-later

#include "catalog/Plan.h"

#include "catalog/Completion.h"

#include <QHash>
#include <QRegularExpression>

namespace sqliteman::catalog {

namespace {

void classify(PlanStep &step)
{
    // SCAN t, SCAN t USING COVERING INDEX i, SEARCH t USING INDEX i (a=?).
    static const QRegularExpression readsTable(QStringLiteral(
        "^(SCAN|SEARCH) (?:TABLE )?(\\S+)(?: AS \\S+)?(?: USING (?:COVERING )?INDEX (\\S+))?"));
    const QRegularExpressionMatch m = readsTable.match(step.detail);
    if (m.hasMatch()) {
        const bool scan = m.captured(1) == QLatin1String("SCAN");
        // A scan through a covering index reads only the index, in order;
        // still every row.
        step.kind = scan ? PlanStep::Kind::Scan : PlanStep::Kind::Search;
        step.object = m.captured(3).isEmpty() ? m.captured(2) : m.captured(3);
        return;
    }
    if (step.detail.startsWith(QLatin1String("USE TEMP B-TREE")))
        step.kind = PlanStep::Kind::TempBTree;
}

void appendText(const Plan &plan, int index, int depth, QString &out)
{
    const PlanStep &step = plan.steps[std::size_t(index)];
    out += QString(depth * 3, QLatin1Char(' ')) + QLatin1String("|--") + step.detail
        + QLatin1Char('\n');
    for (const int child : step.children)
        appendText(plan, child, depth + 1, out);
}

} // namespace

QByteArray explainStatement(const QByteArray &statement)
{
    return "EXPLAIN QUERY PLAN " + statement;
}

std::optional<Plan> parsePlan(const sqlite::RowStore &rows)
{
    if (rows.columnCount() != 4 || rows.column(0).name != QLatin1String("id")
        || rows.column(3).name != QLatin1String("detail"))
        return std::nullopt;
    Plan plan;
    QHash<int, int> byId; // Step id to index in steps.
    for (int row = 0; row < rows.rowCount(); ++row) {
        PlanStep step;
        step.id = rows.valueAt(row, 0).toString().toInt();
        const int parent = rows.valueAt(row, 1).toString().toInt();
        step.detail = rows.valueAt(row, 3).toString();
        classify(step);
        const int index = int(plan.steps.size());
        byId.insert(step.id, index);
        plan.steps.push_back(std::move(step));
        // A parent always comes before its children.
        if (const auto it = byId.constFind(parent); parent != 0 && it != byId.cend())
            plan.steps[std::size_t(*it)].children.push_back(index);
        else
            plan.roots.push_back(index);
    }
    return plan;
}

void nameTables(Plan &plan, const QByteArray &statement)
{
    const QHash<QString, QString> aliases = tableAliases(statement);
    for (PlanStep &step : plan.steps) {
        if (step.object.isEmpty())
            continue;
        // An index has no alias; a table that is its own name stays as it is.
        const auto it = aliases.constFind(step.object.toLower());
        if (it != aliases.cend())
            step.object = it.value();
    }
}

QString planText(const Plan &plan)
{
    QString out = QStringLiteral("QUERY PLAN\n");
    for (const int root : plan.roots)
        appendText(plan, root, 0, out);
    return out;
}

} // namespace sqliteman::catalog
