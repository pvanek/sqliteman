// SPDX-FileCopyrightText: 2007-2026 Petr Vanek
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "catalog/Snapshot.h"
#include "sqlite/Connection.h"
#include "sqlite/Value.h"

#include <QByteArray>
#include <QDateTime>
#include <QHash>
#include <QString>

#include <vector>

class QRandomGenerator;

namespace sqliteman::catalog {

// Fills a table with made-up rows, for trying out queries and indexes on
// more data than there is.
namespace populator {

enum class Action {
    AutoNumber, // The column's maximum plus one, two, ...
    AutoNumberFrom, // value plus one, two, ...
    RandomNumber, // Up to size digits.
    RandomText, // size letters and spaces.
    PrefixedText, // value followed by 1, 2, ...
    Static, // value in every row.
    DateNow, // The time now, as text SQLite's date functions read.
    DateNowUnix,
    DateNowJulian,
    DateRandom, // A random moment since 1970, as text.
    DateRandomUnix,
    DateRandomJulian,
    Ignore, // Left out of the INSERT: its default, or NULL.
};

QString actionName(Action action);
// Whether the action takes value, or size.
bool takesValue(Action action);
bool takesSize(Action action);

struct Column
{
    QString name;
    QString type;
    bool primaryKey = false;
    Action action = Action::Ignore;
    int size = 10; // Digits or characters; from the type when it says, like VARCHAR(20).
    QString value; // For AutoNumberFrom, PrefixedText and Static.
};

// The table's columns with a sensible action each: numbers for an integer
// key, random values by the type otherwise; generated columns ignored.
std::vector<Column> columnsOf(const SchemaObject &table);

// INSERT with a parameter per column that is not ignored; OR IGNORE skips
// the rows that would break a constraint instead of failing.
QByteArray insertStatement(const QString &schema, const QString &table,
                           const std::vector<Column> &columns, bool skipConstraintErrors);
// The maximum of each AutoNumber column, which the rows count on from.
QByteArray maximumQuery(const QString &schema, const QString &table,
                        const std::vector<Column> &columns);

// count rows of parameters for insertStatement(). maxima holds the
// maximum of each AutoNumber column by name; now is the moment the "now"
// actions write.
std::vector<std::vector<sqlite::Value>> rows(const std::vector<Column> &columns, int count,
                                             const QHash<QString, qint64> &maxima,
                                             QRandomGenerator &random, const QDateTime &now);

// Fills the table on the connection's thread, all the rows or none, in a
// savepoint: reads the AutoNumber columns' maxima, makes count rows and
// inserts them. The outcome's value is the number of rows inserted, which
// with skipConstraintErrors may be fewer.
sqlite::TaskOutcome populate(sqlite3 *db, const QString &schema, const QString &table,
                             const std::vector<Column> &columns, int count,
                             bool skipConstraintErrors);

} // namespace populator
} // namespace sqliteman::catalog
