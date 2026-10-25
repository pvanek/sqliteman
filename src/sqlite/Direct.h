// SPDX-FileCopyrightText: 2026 Petr Vanek
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QByteArray>
#include <QString>
#include <QStringList>

#include <functional>
#include <vector>

struct sqlite3;
struct sqlite3_stmt;

// SQL run straight on a handle, for tasks that already are on the
// connection's thread (Connection::perform()). Anywhere else, go through a
// Connection.
namespace sqliteman::sqlite {

class Value;

// Binds values to a statement's parameters by position; a parameter
// without a value gets NULL. Returns SQLite's result code.
int bindValues(sqlite3_stmt *stmt, const std::vector<Value> &values);
// Runs one statement with text parameters ?1, ?2 and so on, calling row for
// each row. Returns the error, empty on success.
QString forEachRow(sqlite3 *db, const QByteArray &sql, const QStringList &parameters,
                   const std::function<void(sqlite3_stmt *)> &row);
// Runs SQL that returns nothing worth reading, several statements allowed.
QString exec(sqlite3 *db, const QByteArray &sql);
// A column of the current row as text; empty for NULL.
QString columnText(sqlite3_stmt *stmt, int column);

} // namespace sqliteman::sqlite
