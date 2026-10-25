// SPDX-FileCopyrightText: 2026 Petr Vanek
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QByteArray>
#include <QByteArrayView>

#include <cstdint>
#include <vector>

namespace sqliteman::sql {

enum class TokenKind : std::uint8_t {
    Whitespace,
    Comment,
    Keyword,
    Identifier,
    QuotedIdentifier, // "name", `name` or [name].
    String,
    Blob, // X'0A1B'
    Number,
    Operator,
    Punctuation, // ( ) , ; .
    Parameter, // ?, ?1, :name, @name, $name
    DotCommand, // .tables and the like of the sqlite3 shell, to the end of the line.
    Unknown,
};

struct Token
{
    TokenKind kind = TokenKind::Unknown;
    qsizetype offset = 0; // In bytes, within the text given to tokenize().
    qsizetype length = 0;

    qsizetype end() const { return offset + length; }
};

// Where the lexer is, so lexing can resume there, e.g. at the start of the
// next line.
struct LexState
{
    enum class Mode : std::uint8_t {
        Code,
        BlockComment,
        String, // '...', also X'...'.
        QuotedIdentifier, // Closed by `close`.
    };

    Mode mode = Mode::Code;
    char close = '"'; // For QuotedIdentifier: ", ` or ].
    bool blob = false; // For String: an X'' literal.
    // Nothing but whitespace and comments since the last semicolon, or the
    // start: a line beginning with a dot there is a shell command.
    bool atStatementStart = true;

    bool operator==(const LexState &) const = default;
};

// Splits text into tokens the way SQLite's own tokenizer does, starting in
// state and leaving the state at the end of the text in it. Lexing a text in
// pieces, like line by line, gives the same tokens as lexing it whole,
// except that tokens spanning pieces are cut at the piece boundaries. Never
// fails: anything unknown becomes an Unknown token.
std::vector<Token> tokenize(QByteArrayView text, LexState &state);

inline std::vector<Token> tokenize(QByteArrayView text)
{
    LexState state;
    return tokenize(text, state);
}

// Whether a token is code, not whitespace or a comment.
inline bool isSignificant(TokenKind kind)
{
    return kind != TokenKind::Whitespace && kind != TokenKind::Comment;
}

// A quoted identifier's name, or a string's text: without the quotes, the
// doubled ones inside undone.
QByteArray unquote(QByteArrayView token);

} // namespace sqliteman::sql
