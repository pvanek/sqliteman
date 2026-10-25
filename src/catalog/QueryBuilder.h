// SPDX-FileCopyrightText: 2007-2026 Petr Vanek
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QString>
#include <QStringList>

#include <vector>

namespace sqliteman::catalog {

// A SELECT put together from lists rather than typed: what the Build Query
// dialog makes.
struct QuerySpec
{
    enum class Relation {
        Contains,
        DoesNotContain,
        StartsWith,
        Equals,
        NotEquals,
        Greater,
        GreaterOrEqual,
        Less,
        LessOrEqual,
        IsNull,
        IsNotNull,
    };

    struct Term
    {
        QString column;
        Relation relation = Relation::Equals;
        QString value; // Unused for IsNull and IsNotNull.
    };

    struct Order
    {
        QString column;
        bool descending = false;
    };

    QString schema;
    QString table;
    bool distinct = false;
    QStringList columns; // Empty: every column, *.
    std::vector<Term> terms;
    bool matchAll = true; // AND between the terms; else OR.
    std::vector<Order> order;
    int limit = 0; // 0: no LIMIT.
};

QString relationName(QuerySpec::Relation relation);
bool takesValue(QuerySpec::Relation relation);

// The SELECT, with names quoted where they need it and values as literals:
// numbers as they are, anything else in quotes. LIKE patterns escape % and
// _ in the value, so they match literally.
QString selectStatement(const QuerySpec &spec);

} // namespace sqliteman::catalog
