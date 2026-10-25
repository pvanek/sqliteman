// SPDX-FileCopyrightText: 2026 Petr Vanek
// SPDX-License-Identifier: GPL-3.0-or-later

#include "ValueLexer.h"

#include "SqlLexer.h"

#include <QFontDatabase>
#include <QGuiApplication>
#include <QPalette>

#include <algorithm>

namespace sqliteman {

using Kind = catalog::ValueToken::Kind;

ValueLexer::ValueLexer(QObject *parent) : QsciLexerCustom(parent)
{
    applyPalette();
}

QString ValueLexer::description(int style) const
{
    if (style == Default)
        return tr("Default");
    switch (Kind(style - Default - 1)) {
    case Kind::Punctuation:
        return tr("Punctuation");
    case Kind::Key:
        return tr("Key");
    case Kind::String:
        return tr("String");
    case Kind::Number:
        return tr("Number");
    case Kind::Literal:
        return tr("Literal");
    case Kind::Error:
        return tr("Error");
    }
    return {};
}

void ValueLexer::setTokens(std::vector<catalog::ValueToken> tokens)
{
    m_tokens = std::move(tokens);
}

void ValueLexer::applyPalette()
{
    const QPalette palette = QGuiApplication::palette();
    const bool dark = palette.color(QPalette::Base).lightness() < 128;
    // The editor's colours, so a JSON string looks like an SQL one.
    auto sqlStyle = [](int style) {
        if (style == Default)
            return int(SqlLexer::Default);
        switch (Kind(style - Default - 1)) {
        case Kind::Punctuation:
            return int(SqlLexer::Punctuation);
        case Kind::Key:
            return int(SqlLexer::QuotedIdentifier);
        case Kind::String:
            return int(SqlLexer::String);
        case Kind::Number:
            return int(SqlLexer::Number);
        case Kind::Literal:
            return int(SqlLexer::Keyword);
        case Kind::Error:
            return int(SqlLexer::Unknown);
        }
        return int(SqlLexer::Default);
    };
    const QFont font = QFontDatabase::systemFont(QFontDatabase::FixedFont);
    setDefaultFont(font);
    setDefaultColor(palette.color(QPalette::Text));
    setDefaultPaper(palette.color(QPalette::Base));
    for (int style = Default; style < StyleCount; ++style) {
        setFont(font, style);
        setColor(SqlLexer::color(sqlStyle(style), dark), style);
        setPaper(palette.color(QPalette::Base), style);
    }
}

void ValueLexer::styleText(int start, int end)
{
    if (!editor())
        return;
    startStyling(start);
    int at = start;
    auto styleTo = [&](qsizetype to, int style) {
        if (to > at) {
            setStyling(int(to - at), style);
            at = int(to);
        }
    };
    // The first token that ends after start.
    auto token = std::ranges::partition_point(
        m_tokens, [start](const catalog::ValueToken &t) { return t.start + t.length <= start; });
    for (; token != m_tokens.end() && token->start < end; ++token) {
        styleTo(std::max<qsizetype>(token->start, start), Default);
        styleTo(std::min<qsizetype>(token->start + token->length, end), styleFor(token->kind));
    }
    styleTo(end, Default);
}

} // namespace sqliteman
