// SPDX-FileCopyrightText: 2026 Petr Vanek
// SPDX-License-Identifier: GPL-3.0-or-later

#include "sql/Splitter.h"

#include "sql/Lexer.h"

#include <algorithm>

namespace sqliteman::sql {

namespace {

bool isWord(QByteArrayView text, const Token &t, const char *word)
{
    return (t.kind == TokenKind::Keyword || t.kind == TokenKind::Identifier)
        && text.sliced(t.offset, t.length).compare(QByteArrayView(word), Qt::CaseInsensitive) == 0;
}

// sqlite3_complete()'s states: only within CREATE TRIGGER does a semicolon
// not end the statement, until the END right after one.
enum class State { Start, Normal, Explain, Create, Trigger, Semi, End };

} // namespace

std::vector<StatementSpan> splitStatements(const QByteArray &utf8Script)
{
    const QByteArrayView text(utf8Script);
    LexState lexState;
    const std::vector<Token> tokens = tokenize(text, lexState);

    std::vector<StatementSpan> out;
    State state = State::Start;
    qsizetype start = -1; // First token of the current statement.
    qsizetype end = 0; // End of its last token.

    const auto finish = [&](bool terminated) {
        if (start >= 0)
            out.push_back({start, end - start, StatementSpan::Kind::Sql, terminated});
        start = -1;
        state = State::Start;
    };

    for (const Token &t : tokens) {
        if (!isSignificant(t.kind))
            continue;
        if (t.kind == TokenKind::DotCommand) {
            finish(false);
            out.push_back({t.offset, t.length, StatementSpan::Kind::DotCommand, true});
            continue;
        }
        const bool semicolon = t.kind == TokenKind::Punctuation && text[t.offset] == ';';
        if (semicolon) {
            switch (state) {
            case State::Trigger:
            case State::Semi:
                state = State::Semi;
                break;
            default:
                finish(true);
                break;
            }
            continue;
        }

        if (start < 0)
            start = t.offset;
        end = t.end();
        switch (state) {
        case State::Start:
            if (isWord(text, t, "explain"))
                state = State::Explain;
            else if (isWord(text, t, "create"))
                state = State::Create;
            else
                state = State::Normal;
            break;
        case State::Explain:
            state = isWord(text, t, "create") ? State::Create : State::Normal;
            break;
        case State::Create:
            if (isWord(text, t, "trigger"))
                state = State::Trigger;
            else if (!isWord(text, t, "temp") && !isWord(text, t, "temporary"))
                state = State::Normal;
            break;
        case State::Semi:
            state = isWord(text, t, "end") ? State::End : State::Trigger;
            break;
        case State::End:
            state = State::Trigger;
            break;
        case State::Normal:
        case State::Trigger:
            break;
        }
    }
    finish(false);
    return out;
}

int statementAt(const QByteArray &utf8Script, const std::vector<StatementSpan> &spans,
                qsizetype pos)
{
    const QByteArrayView text(utf8Script);
    auto sameLine = [&](qsizetype a, qsizetype b) {
        const qsizetype from = std::min(a, b);
        const qsizetype to = std::min(std::max(a, b), text.size());
        return text.sliced(from, to - from).indexOf('\n') < 0;
    };

    int before = -1;
    for (int i = 0; i < int(spans.size()); ++i) {
        const StatementSpan &s = spans[std::size_t(i)];
        qsizetype end = s.offset + s.length;
        if (s.kind == StatementSpan::Kind::Sql && s.terminated) // Include the semicolon.
            end = std::min(text.size(), text.indexOf(';', end) + 1);
        if (pos >= s.offset && pos <= end)
            return i;
        if (end <= pos)
            before = i;
    }
    if (before >= 0) {
        const StatementSpan &s = spans[std::size_t(before)];
        if (sameLine(s.offset + s.length, pos))
            return before;
    }
    const int after = before + 1;
    if (after < int(spans.size()) && sameLine(pos, spans[std::size_t(after)].offset))
        return after;
    return -1;
}

} // namespace sqliteman::sql
