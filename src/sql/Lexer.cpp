// SPDX-FileCopyrightText: 2026 Petr Vanek
// SPDX-License-Identifier: GPL-3.0-or-later

#include "sql/Lexer.h"

#include "sql/Keywords.h"

#include <cstring>

namespace sqliteman::sql {

namespace {

using Mode = LexState::Mode;

bool isSpace(char c)
{
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f';
}

bool isDigit(char c)
{
    return c >= '0' && c <= '9';
}

bool isHexDigit(char c)
{
    return isDigit(c) || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
}

// Bytes >= 0x80 are parts of UTF-8 sequences, which SQLite allows in names.
bool isIdentStart(char c)
{
    const auto u = static_cast<unsigned char>(c);
    return (u >= 'a' && u <= 'z') || (u >= 'A' && u <= 'Z') || u == '_' || u >= 0x80;
}

bool isIdentCont(char c)
{
    return isIdentStart(c) || isDigit(c) || c == '$';
}

class Lexer
{
public:
    Lexer(QByteArrayView text, LexState &state) : m_s(text), m_n(text.size()), m_state(state) { }

    std::vector<Token> run()
    {
        qsizetype i = 0;
        while (i < m_n)
            i = m_state.mode == Mode::Code ? code(i) : resume(i);
        return std::move(m_tokens);
    }

private:
    char peek(qsizetype i) const { return i < m_n ? m_s[i] : '\0'; }

    void emit(TokenKind kind, qsizetype from, qsizetype to)
    {
        if (to <= from)
            return;
        m_tokens.push_back({kind, from, to - from});
        if (isSignificant(kind))
            m_state.atStatementStart = kind == TokenKind::DotCommand
                || (kind == TokenKind::Punctuation && m_s[from] == ';');
    }

    // Continues a construct left open by the previous piece of text.
    qsizetype resume(qsizetype i)
    {
        switch (m_state.mode) {
        case Mode::BlockComment:
            return blockComment(i, i);
        case Mode::String:
            return quoted(i, i, '\'', m_state.blob ? TokenKind::Blob : TokenKind::String);
        case Mode::QuotedIdentifier:
            return quoted(i, i, m_state.close, TokenKind::QuotedIdentifier);
        case Mode::Code:
            break;
        }
        return code(i);
    }

    bool atLineStart(qsizetype i) const { return i == 0 || m_s[i - 1] == '\n'; }

    qsizetype code(qsizetype i)
    {
        const char c = m_s[i];

        if (isSpace(c)) {
            qsizetype end = i + 1;
            while (end < m_n && isSpace(m_s[end]))
                ++end;
            emit(TokenKind::Whitespace, i, end);
            return end;
        }
        if (c == '-' && peek(i + 1) == '-') {
            qsizetype end = i + 2;
            while (end < m_n && m_s[end] != '\n' && m_s[end] != '\r')
                ++end;
            emit(TokenKind::Comment, i, end);
            return end;
        }
        if (c == '/' && peek(i + 1) == '*') {
            m_state.mode = Mode::BlockComment;
            return blockComment(i, i + 2);
        }
        // The sqlite3 shell's commands start a line of their own, outside
        // any statement.
        if (c == '.' && m_state.atStatementStart && atLineStart(i) && (isIdentStart(peek(i + 1)))) {
            qsizetype end = i + 1;
            while (end < m_n && m_s[end] != '\n' && m_s[end] != '\r')
                ++end;
            emit(TokenKind::DotCommand, i, end);
            return end;
        }
        if (c == '\'') {
            m_state.mode = Mode::String;
            m_state.blob = false;
            return quoted(i, i + 1, '\'', TokenKind::String);
        }
        if (c == '"' || c == '`' || c == '[') {
            m_state.mode = Mode::QuotedIdentifier;
            m_state.close = c == '[' ? ']' : c;
            return quoted(i, i + 1, m_state.close, TokenKind::QuotedIdentifier);
        }
        if ((c == 'x' || c == 'X') && peek(i + 1) == '\'') {
            m_state.mode = Mode::String;
            m_state.blob = true;
            return quoted(i, i + 2, '\'', TokenKind::Blob);
        }
        if (isIdentStart(c))
            return word(i);
        if (isDigit(c) || (c == '.' && isDigit(peek(i + 1))))
            return number(i);
        if (c == '?') {
            qsizetype end = i + 1;
            while (end < m_n && isDigit(m_s[end]))
                ++end;
            emit(TokenKind::Parameter, i, end);
            return end;
        }
        if ((c == ':' || c == '@' || c == '$') && isIdentCont(peek(i + 1))) {
            qsizetype end = i + 1;
            while (end < m_n && isIdentCont(m_s[end]))
                ++end;
            emit(TokenKind::Parameter, i, end);
            return end;
        }
        if (std::strchr("(),;.", c)) {
            emit(TokenKind::Punctuation, i, i + 1);
            return i + 1;
        }
        if (const qsizetype end = operatorEnd(i); end > i) {
            emit(TokenKind::Operator, i, end);
            return end;
        }
        emit(TokenKind::Unknown, i, i + 1);
        return i + 1;
    }

    // SQLite's operators are a fixed set, so the longest that fits is the one.
    qsizetype operatorEnd(qsizetype i) const
    {
        static const char *const operators[]
            = {"->>", "||", "<=", ">=", "==", "!=", "<>", "<<", ">>", "->", "=",
               "<",   ">",  "+",  "-",  "*",  "/",  "%",  "&",  "|",  "~"};
        for (const char *op : operators) {
            const auto len = qsizetype(std::strlen(op));
            if (m_s.sliced(i).startsWith(QByteArrayView(op, len)))
                return i + len;
        }
        return i;
    }

    qsizetype blockComment(qsizetype start, qsizetype i)
    {
        // They do not nest in SQLite.
        const qsizetype close = m_s.indexOf(QByteArrayView("*/"), i);
        qsizetype end = m_n;
        if (close >= 0) {
            end = close + 2;
            m_state.mode = Mode::Code;
        }
        emit(TokenKind::Comment, start, end);
        return end;
    }

    // i is past the opening quote, or at the start of a continued piece. A
    // doubled quote is the quote itself; ] cannot be escaped.
    qsizetype quoted(qsizetype start, qsizetype i, char quote, TokenKind kind)
    {
        while (i < m_n) {
            if (m_s[i] == quote) {
                if (quote != ']' && peek(i + 1) == quote) {
                    i += 2;
                    continue;
                }
                ++i;
                m_state.mode = Mode::Code;
                emit(kind, start, i);
                return i;
            }
            ++i;
        }
        emit(kind, start, m_n);
        return m_n;
    }

    qsizetype word(qsizetype i)
    {
        qsizetype end = i + 1;
        while (end < m_n && isIdentCont(m_s[end]))
            ++end;
        emit(isKeyword(m_s.sliced(i, end - i)) ? TokenKind::Keyword : TokenKind::Identifier, i,
             end);
        return end;
    }

    qsizetype number(qsizetype i)
    {
        qsizetype end = i;
        if (m_s[i] == '0' && (peek(i + 1) == 'x' || peek(i + 1) == 'X')
            && isHexDigit(peek(i + 2))) {
            end = i + 2;
            while (end < m_n && (isHexDigit(m_s[end]) || m_s[end] == '_'))
                ++end;
        } else {
            while (end < m_n && (isDigit(m_s[end]) || m_s[end] == '_'))
                ++end;
            if (peek(end) == '.') {
                ++end;
                while (end < m_n && (isDigit(m_s[end]) || m_s[end] == '_'))
                    ++end;
            }
            if ((peek(end) == 'e' || peek(end) == 'E')
                && (isDigit(peek(end + 1))
                    || ((peek(end + 1) == '+' || peek(end + 1) == '-')
                        && isDigit(peek(end + 2))))) {
                end += 2;
                while (end < m_n && isDigit(m_s[end]))
                    ++end;
            }
        }
        emit(TokenKind::Number, i, end);
        return end;
    }

    QByteArrayView m_s;
    qsizetype m_n;
    LexState &m_state;
    std::vector<Token> m_tokens;
};

} // namespace

std::vector<Token> tokenize(QByteArrayView text, LexState &state)
{
    return Lexer(text, state).run();
}

QByteArray unquote(QByteArrayView token)
{
    if (token.size() >= 2 && (token.front() == 'x' || token.front() == 'X') && token[1] == '\'')
        token = token.sliced(1);
    if (token.isEmpty())
        return {};
    const char open = token.front();
    if (open != '"' && open != '\'' && open != '`' && open != '[')
        return token.toByteArray();
    const char close = open == '[' ? ']' : open;
    QByteArray out;
    out.reserve(token.size());
    qsizetype i = 1;
    const qsizetype end
        = token.size() > 1 && token.back() == close ? token.size() - 1 : token.size();
    while (i < end) {
        out += token[i];
        if (close != ']' && token[i] == close && i + 1 < end && token[i + 1] == close)
            ++i;
        ++i;
    }
    return out;
}

} // namespace sqliteman::sql
