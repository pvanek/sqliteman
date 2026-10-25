// SPDX-FileCopyrightText: 2026 Petr Vanek
// SPDX-License-Identifier: GPL-3.0-or-later

#include "ValueFormat.h"

#include <algorithm>

namespace sqliteman::catalog {

namespace {

using Kind = ValueToken::Kind;

bool isSpace(char c)
{
    return c == ' ' || c == '\t' || c == '\n' || c == '\r';
}

QByteArrayView trimmed(QByteArrayView text)
{
    while (!text.isEmpty() && isSpace(text.front()))
        text = text.sliced(1);
    while (!text.isEmpty() && isSpace(text.back()))
        text.chop(1);
    return text;
}

bool isDigit(char c)
{
    return c >= '0' && c <= '9';
}

// Skips digits from i; returns how many there were.
qsizetype digits(QByteArrayView text, qsizetype &i)
{
    const qsizetype start = i;
    while (i < text.size() && isDigit(text[i]))
        ++i;
    return i - start;
}

// JSON's own grammar: no leading zeros, no leading plus, no bare dot.
bool isJsonNumber(QByteArrayView word)
{
    qsizetype i = 0;
    if (i < word.size() && word[i] == '-')
        ++i;
    if (i < word.size() && word[i] == '0')
        ++i;
    else if (digits(word, i) == 0)
        return false;
    if (i < word.size() && word[i] == '.') {
        ++i;
        if (digits(word, i) == 0)
            return false;
    }
    if (i < word.size() && (word[i] == 'e' || word[i] == 'E')) {
        ++i;
        if (i < word.size() && (word[i] == '+' || word[i] == '-'))
            ++i;
        if (digits(word, i) == 0)
            return false;
    }
    return i == word.size();
}

bool isJsonDelimiter(char c)
{
    return isSpace(c) || c == '{' || c == '}' || c == '[' || c == ']' || c == ',' || c == ':'
        || c == '"';
}

// The punctuation character a token is, or 0 for any other token.
char punctuation(const std::vector<ValueToken> &tokens, QByteArrayView text, std::size_t k)
{
    return k < tokens.size() && tokens[k].kind == Kind::Punctuation ? text[tokens[k].start] : 0;
}

// Recursive descent over the tokens. Deeper than this is taken for not
// JSON rather than risking the stack; the value is then shown as it is.
class JsonParser
{
public:
    JsonParser(const std::vector<ValueToken> &tokens, QByteArrayView text)
        : m_tokens(tokens), m_text(text)
    { }

    bool document() { return value(0) && m_i == m_tokens.size(); }

private:
    static constexpr int MaxDepth = 1000;

    char punct() const { return punctuation(m_tokens, m_text, m_i); }

    bool value(int depth)
    {
        if (m_i >= m_tokens.size() || depth > MaxDepth)
            return false;
        const Kind kind = m_tokens[m_i].kind;
        if (kind == Kind::String || kind == Kind::Number || kind == Kind::Literal) {
            ++m_i;
            return true;
        }
        const char open = punct();
        if (open != '[' && open != '{')
            return false;
        const char close = open == '[' ? ']' : '}';
        ++m_i;
        if (punct() == close) {
            ++m_i;
            return true;
        }
        for (;;) {
            if (open == '{') {
                if (m_i >= m_tokens.size() || m_tokens[m_i].kind != Kind::Key)
                    return false;
                ++m_i;
                if (punct() != ':')
                    return false;
                ++m_i;
            }
            if (!value(depth + 1))
                return false;
            if (punct() == ',') {
                ++m_i;
                continue;
            }
            if (punct() != close)
                return false;
            ++m_i;
            return true;
        }
    }

    const std::vector<ValueToken> &m_tokens;
    QByteArrayView m_text;
    std::size_t m_i = 0;
};

} // namespace

std::vector<ValueToken> tokenizeJson(QByteArrayView text)
{
    std::vector<ValueToken> tokens;
    const qsizetype n = text.size();
    qsizetype i = 0;
    while (i < n) {
        const char c = text[i];
        if (isSpace(c)) {
            ++i;
            continue;
        }
        const qsizetype start = i;
        if (c == '{' || c == '}' || c == '[' || c == ']' || c == ',' || c == ':') {
            tokens.push_back({Kind::Punctuation, i++, 1});
            continue;
        }
        if (c == '"') {
            bool closed = false;
            for (++i; i < n && text[i] != '\n'; ++i) {
                if (text[i] == '\\') {
                    ++i;
                } else if (text[i] == '"') {
                    ++i;
                    closed = true;
                    break;
                }
            }
            i = std::min(i, n);
            Kind kind = closed ? Kind::String : Kind::Error;
            if (closed) {
                qsizetype next = i;
                while (next < n && isSpace(text[next]))
                    ++next;
                if (next < n && text[next] == ':')
                    kind = Kind::Key;
            }
            tokens.push_back({kind, start, i - start});
            continue;
        }
        while (i < n && !isJsonDelimiter(text[i]))
            ++i;
        if (i == start)
            ++i; // A character no rule takes, such as a NUL.
        const QByteArrayView word = text.sliced(start, i - start);
        const Kind kind = isJsonNumber(word)                      ? Kind::Number
            : word == "true" || word == "false" || word == "null" ? Kind::Literal
                                                                  : Kind::Error;
        tokens.push_back({kind, start, i - start});
    }
    return tokens;
}

std::vector<ValueToken> tokenize(ValueSyntax syntax, QByteArrayView text)
{
    switch (syntax) {
    case ValueSyntax::Json:
        return tokenizeJson(text);
    case ValueSyntax::Text:
        break;
    }
    return {};
}

bool isValidJson(QByteArrayView text)
{
    const std::vector<ValueToken> tokens = tokenizeJson(text);
    return JsonParser(tokens, text).document();
}

std::optional<QByteArray> prettyJson(QByteArrayView text)
{
    const std::vector<ValueToken> tokens = tokenizeJson(text);
    if (!JsonParser(tokens, text).document())
        return std::nullopt;

    QByteArray out;
    out.reserve(text.size() * 2);
    int depth = 0;
    auto newLine = [&] {
        out += '\n';
        out.append(depth * 4, ' ');
    };
    for (std::size_t k = 0; k < tokens.size(); ++k) {
        const ValueToken &token = tokens[k];
        if (token.kind != Kind::Punctuation) {
            out += text.sliced(token.start, token.length);
            continue;
        }
        switch (const char c = text[token.start]) {
        case '{':
        case '[':
            out += c;
            if (const char close = c == '{' ? '}' : ']';
                punctuation(tokens, text, k + 1) == close) {
                out += close; // Empty: {} and [] as they are.
                ++k;
            } else {
                ++depth;
                newLine();
            }
            break;
        case '}':
        case ']':
            --depth;
            newLine();
            out += c;
            break;
        case ',':
            out += c;
            newLine();
            break;
        case ':':
            out += ": ";
            break;
        }
    }
    return out;
}

std::optional<QByteArray> pretty(ValueSyntax syntax, QByteArrayView text)
{
    switch (syntax) {
    case ValueSyntax::Json:
        return prettyJson(text);
    case ValueSyntax::Text:
        break;
    }
    return std::nullopt;
}

ValueSyntax guessSyntax(QByteArrayView value)
{
    const QByteArrayView v = trimmed(value);
    if (v.isEmpty())
        return ValueSyntax::Text;
    // SQLite keeps JSON as text, so the value is what tells.
    if ((v.front() == '{' || v.front() == '[') && isValidJson(v))
        return ValueSyntax::Json;
    return ValueSyntax::Text;
}

} // namespace sqliteman::catalog
