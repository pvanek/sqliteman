// SPDX-FileCopyrightText: 2007-2026 Petr Vanek
// SPDX-License-Identifier: GPL-3.0-or-later

#include "catalog/Populator.h"

#include "sql/Keywords.h"
#include "sqlite/Direct.h"
#include "sqlite/Handles.h"

#include <QCoreApplication>
#include <QRandomGenerator>
#include <QRegularExpression>

#include <cmath>

namespace sqliteman::catalog::populator {

namespace {

QString tr(const char *text)
{
    return QCoreApplication::translate("Populator", text);
}

QString qualified(const QString &schema, const QString &table)
{
    return sql::identifier(schema.isEmpty() ? QStringLiteral("main") : schema) + QLatin1Char('.')
        + sql::identifier(table);
}

// The Julian day number of a Unix time, as SQLite's julianday() counts.
double julian(qint64 unixSeconds)
{
    return double(unixSeconds) / 86400.0 + 2440587.5;
}

QString dateText(const QDateTime &time)
{
    return time.toUTC().toString(QStringLiteral("yyyy-MM-dd HH:mm:ss"));
}

} // namespace

QString actionName(Action action)
{
    switch (action) {
    case Action::AutoNumber:
        return tr("Autonumber");
    case Action::AutoNumberFrom:
        return tr("Autonumber from value");
    case Action::RandomNumber:
        return tr("Random number");
    case Action::RandomText:
        return tr("Random text");
    case Action::PrefixedText:
        return tr("Prefixed text");
    case Action::Static:
        return tr("Static value");
    case Action::DateNow:
        return tr("Date: now");
    case Action::DateNowUnix:
        return tr("Date: now, Unix time");
    case Action::DateNowJulian:
        return tr("Date: now, Julian day");
    case Action::DateRandom:
        return tr("Date: random");
    case Action::DateRandomUnix:
        return tr("Date: random, Unix time");
    case Action::DateRandomJulian:
        return tr("Date: random, Julian day");
    case Action::Ignore:
        return tr("Ignore");
    }
    return {};
}

bool takesValue(Action action)
{
    return action == Action::AutoNumberFrom || action == Action::PrefixedText
        || action == Action::Static;
}

bool takesSize(Action action)
{
    return action == Action::RandomNumber || action == Action::RandomText;
}

std::vector<Column> columnsOf(const SchemaObject &table)
{
    static const QRegularExpression sizeInType(QStringLiteral("\\((\\d+)"));
    std::vector<Column> out;
    for (const ColumnInfo &info : table.columns) {
        Column c;
        c.name = info.name;
        c.type = info.type;
        c.primaryKey = info.primaryKey > 0;
        if (const QRegularExpressionMatch m = sizeInType.match(info.type); m.hasMatch())
            c.size = std::clamp(m.captured(1).toInt(), 1, 1000);
        const QString type = info.type.toUpper();
        if (info.isGenerated() || info.hidden == 1)
            c.action = Action::Ignore;
        else if (c.primaryKey && affinity(info.type) == Affinity::Integer)
            c.action = Action::AutoNumber;
        else if (type.contains(QLatin1String("DATE")) || type.contains(QLatin1String("TIME")))
            c.action = Action::DateRandom;
        else if (affinity(info.type) == Affinity::Text || affinity(info.type) == Affinity::Blob)
            c.action = Action::RandomText;
        else
            c.action = Action::RandomNumber;
        out.push_back(std::move(c));
    }
    return out;
}

QByteArray insertStatement(const QString &schema, const QString &table,
                           const std::vector<Column> &columns, bool skipConstraintErrors)
{
    QStringList names;
    QStringList parameters;
    for (const Column &c : columns) {
        if (c.action == Action::Ignore)
            continue;
        names << sql::identifier(c.name);
        parameters << QStringLiteral("?");
    }
    const QString verb
        = skipConstraintErrors ? QStringLiteral("INSERT OR IGNORE") : QStringLiteral("INSERT");
    if (names.isEmpty())
        return (verb + QLatin1String(" INTO ") + qualified(schema, table)
                + QLatin1String(" DEFAULT VALUES"))
            .toUtf8();
    return QStringLiteral("%1 INTO %2 (%3) VALUES (%4)")
        .arg(verb, qualified(schema, table), names.join(QLatin1String(", ")),
             parameters.join(QLatin1String(", ")))
        .toUtf8();
}

QByteArray maximumQuery(const QString &schema, const QString &table,
                        const std::vector<Column> &columns)
{
    QStringList maxima;
    for (const Column &c : columns) {
        if (c.action == Action::AutoNumber)
            maxima << QStringLiteral("coalesce(max(%1), 0) AS %1").arg(sql::identifier(c.name));
    }
    if (maxima.isEmpty())
        return {};
    return QStringLiteral("SELECT %1 FROM %2")
        .arg(maxima.join(QLatin1String(", ")), qualified(schema, table))
        .toUtf8();
}

std::vector<std::vector<sqlite::Value>> rows(const std::vector<Column> &columns, int count,
                                             const QHash<QString, qint64> &maxima,
                                             QRandomGenerator &random, const QDateTime &now)
{
    using sqlite::Value;
    static const QString letters
        = QStringLiteral("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz     ");
    const qint64 nowSeconds = now.toSecsSinceEpoch();

    std::vector<std::vector<Value>> out;
    out.reserve(std::size_t(std::max(count, 0)));
    for (int i = 0; i < count; ++i) {
        std::vector<Value> row;
        for (const Column &c : columns) {
            const qint64 randomSeconds
                = nowSeconds > 0 ? qint64(random.bounded(quint64(nowSeconds))) : 0;
            switch (c.action) {
            case Action::AutoNumber:
                row.push_back(Value::integer(maxima.value(c.name) + i + 1));
                break;
            case Action::AutoNumberFrom:
                row.push_back(Value::integer(c.value.toLongLong() + i + 1));
                break;
            case Action::RandomNumber: {
                // Below 10^size, and within 64 bits.
                const int digits = std::clamp(c.size, 1, 18);
                const auto limit = quint64(std::pow(10.0, digits));
                row.push_back(Value::integer(qint64(random.bounded(limit))));
                break;
            }
            case Action::RandomText: {
                QString text;
                text.reserve(c.size);
                for (int j = 0; j < c.size; ++j)
                    text += letters.at(int(random.bounded(quint32(letters.size()))));
                row.push_back(Value::text(text.simplified()));
                break;
            }
            case Action::PrefixedText:
                row.push_back(Value::text(c.value + QString::number(i + 1)));
                break;
            case Action::Static:
                row.push_back(Value::fromUserText(c.value));
                break;
            case Action::DateNow:
                row.push_back(Value::text(dateText(now)));
                break;
            case Action::DateNowUnix:
                row.push_back(Value::integer(nowSeconds));
                break;
            case Action::DateNowJulian:
                row.push_back(Value::real(julian(nowSeconds)));
                break;
            case Action::DateRandom:
                row.push_back(Value::text(dateText(QDateTime::fromSecsSinceEpoch(randomSeconds))));
                break;
            case Action::DateRandomUnix:
                row.push_back(Value::integer(randomSeconds));
                break;
            case Action::DateRandomJulian:
                row.push_back(Value::real(julian(randomSeconds)));
                break;
            case Action::Ignore:
                break;
            }
        }
        out.push_back(std::move(row));
    }
    return out;
}

sqlite::TaskOutcome populate(sqlite3 *db, const QString &schema, const QString &table,
                             const std::vector<Column> &columns, int count,
                             bool skipConstraintErrors)
{
    sqlite::TaskOutcome outcome;
    QHash<QString, qint64> maxima;
    if (const QByteArray query = maximumQuery(schema, table, columns); !query.isEmpty()) {
        outcome.error = sqlite::forEachRow(db, query, {}, [&](sqlite3_stmt *s) {
            int c = 0;
            for (const Column &column : columns) {
                if (column.action == Action::AutoNumber)
                    maxima.insert(column.name, sqlite3_column_int64(s, c++));
            }
        });
        if (!outcome.error.isEmpty())
            return outcome;
    }
    const QByteArray insert = insertStatement(schema, table, columns, skipConstraintErrors);
    sqlite3_stmt *raw = nullptr;
    if (sqlite3_prepare_v2(db, insert.constData(), int(insert.size()), &raw, nullptr)
        != SQLITE_OK) {
        outcome.error = QString::fromUtf8(sqlite3_errmsg(db));
        return outcome;
    }
    sqlite::StatementPtr stmt(raw);
    outcome.error = sqlite::exec(db, "SAVEPOINT sqliteman_populate");
    if (!outcome.error.isEmpty())
        return outcome;
    const qint64 before = sqlite3_total_changes64(db);
    const auto values = rows(columns, count, maxima, *QRandomGenerator::global(),
                             QDateTime::currentDateTimeUtc());
    for (const std::vector<sqlite::Value> &row : values) {
        sqlite3_reset(stmt.get());
        sqlite3_clear_bindings(stmt.get());
        if (sqlite::bindValues(stmt.get(), row) != SQLITE_OK
            || sqlite3_step(stmt.get()) != SQLITE_DONE) {
            outcome.error = QString::fromUtf8(sqlite3_errmsg(db));
            break;
        }
    }
    const qint64 inserted = sqlite3_total_changes64(db) - before;
    if (outcome.error.isEmpty()) {
        outcome.error = sqlite::exec(db, "RELEASE sqliteman_populate");
        outcome.value = inserted;
    } else {
        sqlite::exec(db, "ROLLBACK TO sqliteman_populate");
        sqlite::exec(db, "RELEASE sqliteman_populate");
        outcome.value = 0;
    }
    return outcome;
}

} // namespace sqliteman::catalog::populator
