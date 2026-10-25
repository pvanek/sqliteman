// SPDX-FileCopyrightText: 2007-2026 Petr Vanek
// SPDX-License-Identifier: GPL-3.0-or-later

#include "catalog/QueryBuilder.h"

#include "sql/Keywords.h"
#include "sqlite/Quote.h"
#include "sqlite/Value.h"

#include <QCoreApplication>

namespace sqliteman::catalog {

namespace {

using Relation = QuerySpec::Relation;

QString valueLiteral(const QString &value)
{
    return sqlite::literal(sqlite::Value::fromUserText(value));
}

QString likePattern(const QString &before, const QString &value, const QString &after)
{
    QString escaped = value;
    escaped.replace(QLatin1Char('\\'), QLatin1String("\\\\"));
    escaped.replace(QLatin1Char('%'), QLatin1String("\\%"));
    escaped.replace(QLatin1Char('_'), QLatin1String("\\_"));
    return sqlite::quoteLiteral(before + escaped + after) + QLatin1String(" ESCAPE '\\'");
}

} // namespace

QString relationName(Relation relation)
{
    const auto tr
        = [](const char *text) { return QCoreApplication::translate("QueryBuilder", text); };
    switch (relation) {
    case Relation::Contains:
        return tr("contains");
    case Relation::DoesNotContain:
        return tr("does not contain");
    case Relation::StartsWith:
        return tr("starts with");
    case Relation::Equals:
        return QStringLiteral("=");
    case Relation::NotEquals:
        return QStringLiteral("<>");
    case Relation::Greater:
        return QStringLiteral(">");
    case Relation::GreaterOrEqual:
        return QStringLiteral(">=");
    case Relation::Less:
        return QStringLiteral("<");
    case Relation::LessOrEqual:
        return QStringLiteral("<=");
    case Relation::IsNull:
        return tr("is NULL");
    case Relation::IsNotNull:
        return tr("is not NULL");
    }
    return {};
}

bool takesValue(Relation relation)
{
    return relation != Relation::IsNull && relation != Relation::IsNotNull;
}

QString selectStatement(const QuerySpec &spec)
{
    QString sql = QStringLiteral("SELECT ");
    if (spec.distinct)
        sql += QLatin1String("DISTINCT ");
    if (spec.columns.isEmpty()) {
        sql += QLatin1Char('*');
    } else {
        QStringList names;
        for (const QString &c : spec.columns)
            names << sql::identifier(c);
        sql += names.join(QLatin1String(", "));
    }
    sql += QLatin1String("\nFROM ");
    if (!spec.schema.isEmpty() && spec.schema != QLatin1String("main"))
        sql += sql::identifier(spec.schema) + QLatin1Char('.');
    sql += sql::identifier(spec.table);

    QStringList conditions;
    for (const QuerySpec::Term &t : spec.terms) {
        if (t.column.isEmpty())
            continue;
        const QString column = sql::identifier(t.column);
        switch (t.relation) {
        case Relation::Contains:
            conditions << column + QLatin1String(" LIKE ")
                    + likePattern(QStringLiteral("%"), t.value, QStringLiteral("%"));
            break;
        case Relation::DoesNotContain:
            conditions << column + QLatin1String(" NOT LIKE ")
                    + likePattern(QStringLiteral("%"), t.value, QStringLiteral("%"));
            break;
        case Relation::StartsWith:
            conditions << column + QLatin1String(" LIKE ")
                    + likePattern(QString(), t.value, QStringLiteral("%"));
            break;
        case Relation::IsNull:
            conditions << column + QLatin1String(" IS NULL");
            break;
        case Relation::IsNotNull:
            conditions << column + QLatin1String(" IS NOT NULL");
            break;
        default:
            conditions << column + QLatin1Char(' ') + relationName(t.relation) + QLatin1Char(' ')
                    + valueLiteral(t.value);
            break;
        }
    }
    if (!conditions.isEmpty())
        sql += QLatin1String("\nWHERE ")
            + conditions.join(spec.matchAll ? QLatin1String("\n  AND ")
                                            : QLatin1String("\n   OR "));

    QStringList order;
    for (const QuerySpec::Order &o : spec.order) {
        if (!o.column.isEmpty())
            order << sql::identifier(o.column)
                    + (o.descending ? QLatin1String(" DESC") : QString());
    }
    if (!order.isEmpty())
        sql += QLatin1String("\nORDER BY ") + order.join(QLatin1String(", "));
    if (spec.limit > 0)
        sql += QLatin1String("\nLIMIT ") + QString::number(spec.limit);
    return sql;
}

} // namespace sqliteman::catalog
