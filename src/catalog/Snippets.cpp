// SPDX-FileCopyrightText: 2026 Petr Vanek
// SPDX-License-Identifier: GPL-3.0-or-later

#include "catalog/Snippets.h"

#include <QCoreApplication>

namespace sqliteman::catalog {

const std::vector<Snippet> &snippets()
{
    // Keywords are written in capitals here; completion lowercases the lot
    // when that is how the abbreviation was typed.
    static const std::vector<Snippet> all = [] {
        auto title = [](const char *text) {
            return QCoreApplication::translate("sqliteman::catalog", text);
        };
        return std::vector<Snippet> {
            {QStringLiteral("sf"), QStringLiteral("SELECT * FROM $|"), title("Select everything")},
            {QStringLiteral("scf"), QStringLiteral("SELECT count(1) FROM $|"),
             title("Count the rows")},
            {QStringLiteral("sfw"), QStringLiteral("SELECT * FROM $| WHERE "),
             title("Select with a condition")},
            {QStringLiteral("sfl"), QStringLiteral("SELECT * FROM $| LIMIT 100"),
             title("Select the first rows")},
            {QStringLiteral("ins"), QStringLiteral("INSERT INTO $| () VALUES ()"),
             title("Insert a row")},
            {QStringLiteral("upd"), QStringLiteral("UPDATE $| SET  WHERE "), title("Update rows")},
            {QStringLiteral("del"), QStringLiteral("DELETE FROM $| WHERE "), title("Delete rows")},
            {QStringLiteral("ct"), QStringLiteral("CREATE TABLE $| (\n)"), title("Create a table")},
            {QStringLiteral("ctas"), QStringLiteral("CREATE TABLE $| AS\nSELECT "),
             title("Create a table from a query")},
            {QStringLiteral("cte"), QStringLiteral("WITH q AS (\n    $|\n)\nSELECT * FROM q"),
             title("Query with a WITH clause")},
            // SQLite's EXPLAIN takes no options; the plan is what is wanted.
            {QStringLiteral("eqp"), QStringLiteral("EXPLAIN QUERY PLAN $|"),
             title("Explain the query plan")},
            {QStringLiteral("tx"), QStringLiteral("BEGIN;\n$|\nCOMMIT;"), title("Transaction")},
        };
    }();
    return all;
}

} // namespace sqliteman::catalog
