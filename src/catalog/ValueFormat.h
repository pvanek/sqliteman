// SPDX-FileCopyrightText: 2026 Petr Vanek
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QByteArray>
#include <QByteArrayView>

#include <optional>
#include <vector>

namespace sqliteman::catalog {

// How a single value from a result reads: the value viewer formats and
// highlights by it.
enum class ValueSyntax {
    Text,
    Json,
};

// JSON when the value is a JSON object or array, as SQLite's JSON
// functions write it; text otherwise.
ValueSyntax guessSyntax(QByteArrayView value);

struct ValueToken
{
    enum Kind {
        Punctuation, // Braces, brackets, commas, colons.
        Key, // A JSON object's member name.
        String,
        Number,
        Literal, // true, false and null.
        Error, // What does not belong where it is.
    };
    Kind kind;
    qsizetype start; // In bytes of the UTF-8 text.
    qsizetype length;
};

// Whitespace between tokens is not one. Nothing is left out otherwise, so
// even text that is not JSON gets tokens to show it with, errors included.
std::vector<ValueToken> tokenizeJson(QByteArrayView text);
std::vector<ValueToken> tokenize(ValueSyntax syntax, QByteArrayView text);

bool isValidJson(QByteArrayView text);

// One member or element per line, indented by four spaces as json_pretty()
// does, with the members in the order they came, and numbers as they were
// written. Nullopt for text that is not JSON.
std::optional<QByteArray> prettyJson(QByteArrayView text);
std::optional<QByteArray> pretty(ValueSyntax syntax, QByteArrayView text);

} // namespace sqliteman::catalog
