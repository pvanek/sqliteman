// SPDX-FileCopyrightText: 2026 Petr Vanek
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "sqlite/Value.h"

#include <QString>

namespace sqliteman::sqlite {

// "name", with double quotes inside doubled. Always quoted: SQLite takes
// any name that way, keywords and odd characters included, and keeps its
// case as written.
QString quoteIdentifier(const QString &name);
// "schema"."name"; the schema left out when empty.
QString qualifiedName(const QString &schema, const QString &name);
// 'text', with single quotes inside doubled.
QString quoteLiteral(const QString &text);
// X'0A1B', the literal for a blob.
QString blobLiteral(const QByteArray &bytes);
// The literal that gives back the value: NULL, a number as it is, quoted
// text or a blob.
QString literal(const Value &value);

} // namespace sqliteman::sqlite
