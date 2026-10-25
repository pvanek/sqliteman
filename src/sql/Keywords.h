// SPDX-FileCopyrightText: 2026 Petr Vanek
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QByteArray>
#include <QByteArrayView>
#include <QString>
#include <QStringList>

#include <vector>

namespace sqliteman::sql {

// SQLite's keywords, as the linked library reports them
// (sqlite3_keyword_name()), so they always match the parser that runs the
// SQL.

// Whether a word is a keyword, case-insensitively.
bool isKeyword(QByteArrayView word);

// All keywords, uppercase and sorted.
std::vector<QByteArray> keywords();

// Type names as SQLite's documentation spells them out: STRICT tables take
// only the first few, any other table any of them, with the affinity its
// name gives.
QStringList typeNames();

// Whether a name must be quoted to be used as an identifier: it is not a
// letter or _ followed by letters, digits and _, or it is a keyword.
bool needsQuoting(QByteArrayView identifier);

// The name as it goes into SQL: as it is when it can, else in double quotes.
QString identifier(const QString &name);

} // namespace sqliteman::sql
