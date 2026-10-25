// SPDX-FileCopyrightText: 2026 Petr Vanek
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QByteArray>

#include <vector>

namespace sqliteman::sql {

// A piece of a script, as a span of the UTF-8 source in bytes.
struct StatementSpan
{
    enum class Kind {
        // An SQL statement. The span starts at the first token and ends after
        // the last one: surrounding whitespace, leading and trailing comments
        // and the terminating semicolon are not included.
        Sql,
        // A command of the sqlite3 shell such as .tables or .mode, a line of
        // its own; not SQL, so it is not run.
        DotCommand,
    };

    qsizetype offset = 0;
    qsizetype length = 0;
    Kind kind = Kind::Sql;
    // Sql: ended with a semicolon. False when the script cuts the statement
    // short, including in the middle of a string, comment or trigger body.
    // DotCommand: always true.
    bool terminated = false;
};

// Splits a script into statements the way sqlite3_complete() sees them,
// without parsing it. Semicolons inside quotes, comments and the BEGIN ...
// END body of CREATE TRIGGER do not end a statement. Never fails:
// incomplete input yields an unterminated last span, so it is safe to call
// on text being edited. Spans are returned in script order.
std::vector<StatementSpan> splitStatements(const QByteArray &utf8Script);

// The span "run the current statement" means for a cursor at byte pos: the
// one the cursor is in (including its semicolon), else the one ending or
// starting on the cursor's line, preferring the one before. Returns an index
// into spans, or -1.
int statementAt(const QByteArray &utf8Script, const std::vector<StatementSpan> &spans,
                qsizetype pos);

} // namespace sqliteman::sql
