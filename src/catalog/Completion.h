// SPDX-FileCopyrightText: 2026 Petr Vanek
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "catalog/Snapshot.h"

#include <QByteArray>
#include <QHash>
#include <QString>

#include <vector>

namespace sqliteman::catalog {

struct CompletionItem
{
    enum class Kind {
        Keyword,
        Schema, // main, temp or an attached database.
        Table,
        View,
        Alias, // A name in the statement that stands for a table.
        Column,
        Function,
        Pragma,
        Snippet, // A short word standing for a piece of SQL, like "sf".
    };

    Kind kind = Kind::Keyword;
    QString label; // What the list shows.
    QString insertText; // What goes into the text, quoted if needed.
    QString detail; // Like a column's type, or a table's database.
    // Where the caret goes once the text is in, counted in characters from
    // its start; -1 leaves it at the end. A function's goes between its
    // parentheses, a snippet's where the rest is to be written.
    int caret = -1;
    int score = 0;
};

struct Completion
{
    enum class Context {
        None, // In a comment or string: nothing to offer.
        Keyword, // After a complete expression or name, like "FROM t |".
        Relation, // After FROM, JOIN, UPDATE, INTO, ...
        Column, // In a select list, WHERE, ON, ...
        Qualified, // After "x.": columns of x, or objects in database x.
        Pragma, // After PRAGMA.
    };

    Context context = Context::None;
    // What to replace, in bytes within the statement: the word at the
    // cursor, or an empty range at it.
    qsizetype replaceFrom = 0;
    qsizetype replaceTo = 0;
    QString prefix; // The part of the word before the cursor.
    std::vector<CompletionItem> items; // Best first.
};

// Suggestions for the cursor at byte offset cursor in one SQL statement,
// from the tokens around it: which clause it is in, and the tables the
// statement names. Keywords and snippets come in lower case when what was
// typed is all lower case, in capitals otherwise. Thread-safe: runs off the
// UI thread.
Completion complete(const QByteArray &statement, qsizetype cursor, const Snapshot &snapshot);

// The tables a statement names after FROM, JOIN, UPDATE and INTO, keyed by
// the alias they go by, lower case: "c" for "FROM Customer c". A table with
// no alias is there under its own name.
QHash<QString, QString> tableAliases(const QByteArray &statement);

// How well a candidate matches what was typed: 0 for no match, higher for
// better (prefix, then word starts like "cnm" for customer_name, then a
// substring, then any subsequence). Case-insensitive.
int fuzzyScore(const QString &typed, const QString &candidate);

// Which letters of candidate fuzzyScore() matched, in order, for showing
// what the suggestion has in common with what was typed. Empty when they do
// not match at all.
std::vector<qsizetype> fuzzyMatchPositions(const QString &typed, const QString &candidate);

} // namespace sqliteman::catalog
