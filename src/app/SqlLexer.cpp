// SPDX-FileCopyrightText: 2026 Petr Vanek
// SPDX-License-Identifier: GPL-3.0-or-later

#include "SqlLexer.h"

#include "sql/Keywords.h"

#include "Qsci.h"

#include <QFontDatabase>
#include <QGuiApplication>
#include <QPalette>
#include <QStringList>

namespace sqliteman {

using sql::TokenKind;

namespace {

bool isTypeName(QByteArrayView word)
{
    static const QStringList types = sql::typeNames();
    return types.contains(QString::fromLatin1(word), Qt::CaseInsensitive);
}

} // namespace

// Bit layout of a line state: mode (2), quote (2), blob (1), at a
// statement's start (1).
int SqlLexer::encode(const sql::LexState &s)
{
    int quote = 0;
    if (s.close == '`')
        quote = 1;
    else if (s.close == ']')
        quote = 2;
    return int(s.mode) | quote << 2 | (s.blob ? 1 : 0) << 4 | (s.atStatementStart ? 1 : 0) << 5;
}

sql::LexState SqlLexer::decode(int v)
{
    sql::LexState s;
    s.mode = sql::LexState::Mode(v & 3);
    const int quote = (v >> 2) & 3;
    s.close = quote == 1 ? '`' : quote == 2 ? ']' : '"';
    s.blob = (v >> 4) & 1;
    s.atStatementStart = (v >> 5) & 1;
    return s;
}

SqlLexer::SqlLexer(QObject *parent)
    : QsciLexerCustom(parent), m_font(QFontDatabase::systemFont(QFontDatabase::FixedFont))
{
    m_dark = QGuiApplication::palette().color(QPalette::Base).lightness() < 128;
}

void SqlLexer::refreshPalette()
{
    m_dark = QGuiApplication::palette().color(QPalette::Base).lightness() < 128;
}

QString SqlLexer::description(int style) const
{
    switch (style) {
    case Default:
        return tr("Default");
    case Comment:
        return tr("Comment");
    case Keyword:
        return tr("Keyword");
    case Type:
        return tr("Type");
    case Identifier:
        return tr("Identifier");
    case QuotedIdentifier:
        return tr("Quoted identifier");
    case String:
        return tr("String");
    case Blob:
        return tr("Blob");
    case Number:
        return tr("Number");
    case Operator:
        return tr("Operator");
    case Punctuation:
        return tr("Punctuation");
    case Parameter:
        return tr("Parameter");
    case DotCommand:
        return tr("Shell command");
    case Unknown:
        return tr("Unknown");
    default:
        return {};
    }
}

SqlLexer::Style SqlLexer::styleFor(const sql::Token &t, QByteArrayView text)
{
    switch (t.kind) {
    case TokenKind::Whitespace:
        return Default;
    case TokenKind::Comment:
        return Comment;
    case TokenKind::Keyword:
        return Keyword;
    case TokenKind::Identifier:
        return isTypeName(text.sliced(t.offset, t.length)) ? Type : Identifier;
    case TokenKind::QuotedIdentifier:
        return QuotedIdentifier;
    case TokenKind::String:
        return String;
    case TokenKind::Blob:
        return Blob;
    case TokenKind::Number:
        return Number;
    case TokenKind::Operator:
        return Operator;
    case TokenKind::Punctuation:
        return Punctuation;
    case TokenKind::Parameter:
        return Parameter;
    case TokenKind::DotCommand:
        return DotCommand;
    case TokenKind::Unknown:
        return Unknown;
    }
    return Default;
}

int SqlLexer::stateDepth(const sql::LexState &s)
{
    return s.mode != sql::LexState::Mode::Code ? 1 : 0;
}

int SqlLexer::foldDepthAfter(const std::vector<sql::Token> &tokens, QByteArrayView text, int depth)
{
    std::vector<const sql::Token *> significant;
    for (const sql::Token &t : tokens) {
        if (sql::isSignificant(t.kind))
            significant.push_back(&t);
    }
    auto word = [&](std::size_t i) -> QByteArray {
        if (i >= significant.size())
            return {};
        const sql::Token &t = *significant[i];
        if (t.kind != TokenKind::Keyword && t.kind != TokenKind::Identifier)
            return {};
        return text.sliced(t.offset, t.length).toByteArray().toLower();
    };

    for (std::size_t i = 0; i < significant.size(); ++i) {
        const sql::Token &t = *significant[i];
        if (t.kind == TokenKind::DotCommand)
            continue; // Not part of any statement.
        if (depth == 0)
            depth = 1; // A statement starts.
        if (t.kind == TokenKind::Punctuation) {
            const char c = text[t.offset];
            if (c == '(')
                ++depth;
            else if (c == ')' && depth > 1)
                --depth;
            else if (c == ';' && depth == 1)
                depth = 0; // The statement ends.
            continue;
        }
        const QByteArray w = word(i);
        if (w == "case" || (w == "begin" && depth >= 1 && i > 0)) {
            // BEGIN opens a trigger's body; a BEGIN on its own is a transaction.
            ++depth;
        } else if (w == "end" && depth > 1) {
            --depth;
        }
    }
    return depth;
}

void SqlLexer::styleText(int start, int end)
{
    QsciScintilla *e = editor();
    if (!e)
        return;
    const long length = e->SendScintilla(QsciScintillaBase::SCI_GETLENGTH);
    int line = int(e->SendScintilla(QsciScintillaBase::SCI_LINEFROMPOSITION, start));
    const int lastLine = int(e->SendScintilla(QsciScintillaBase::SCI_LINEFROMPOSITION, end));
    const int lineCount = e->lines();

    // A pointer result: SendScintilla() returns long, too small on 64-bit Windows.
    const auto *document = static_cast<const char *>(
        e->SendScintillaPtrResult(QsciScintillaBase::SCI_GETCHARACTERPOINTER));
    sql::LexState state = line > 0
        ? decode(int(e->SendScintilla(QsciScintillaBase::SCI_GETLINESTATE, line - 1)))
        : sql::LexState();

    constexpr int Base = QsciScintillaBase::SC_FOLDLEVELBASE;
    constexpr int NumberMask = QsciScintillaBase::SC_FOLDLEVELNUMBERMASK;
    constexpr int Header = QsciScintillaBase::SC_FOLDLEVELHEADERFLAG;
    auto level
        = [e](int l) { return int(e->SendScintilla(QsciScintillaBase::SCI_GETFOLDLEVEL, l)); };
    int structure
        = line > 0 ? std::max(0, (level(line) & NumberMask) - Base - stateDepth(state)) : 0;

    for (; line < lineCount; ++line) {
        const long from = e->SendScintilla(QsciScintillaBase::SCI_POSITIONFROMLINE, line);
        const long to = line + 1 < lineCount
            ? e->SendScintilla(QsciScintillaBase::SCI_POSITIONFROMLINE, line + 1)
            : length;
        const QByteArrayView text(document + from, to - from);

        startStyling(int(from));
        const int startDepth = structure + stateDepth(state);
        const std::vector<sql::Token> tokens = sql::tokenize(text, state);
        bool afterDot = false; // In t.name, name is a column, not a keyword or type.
        for (const sql::Token &t : tokens) {
            Style style = styleFor(t, text);
            if (afterDot && (style == Keyword || style == Type))
                style = Identifier;
            if (sql::isSignificant(t.kind))
                afterDot = t.kind == TokenKind::Punctuation && text[t.offset] == '.';
            setStyling(int(t.length), style);
        }

        structure = foldDepthAfter(tokens, text, structure);
        const int endDepth = structure + stateDepth(state);
        e->SendScintilla(QsciScintillaBase::SCI_SETFOLDLEVEL, line,
                         (Base + startDepth) | (endDepth > startDepth ? Header : 0));
        int nextLevel = Base + endDepth;
        if (line + 1 < lineCount) {
            nextLevel |= level(line + 1) & Header;
            const int previousNext = level(line + 1);
            e->SendScintilla(QsciScintillaBase::SCI_SETFOLDLEVEL, line + 1, nextLevel);
            nextLevel = previousNext == nextLevel ? nextLevel : -1;
        }

        const int encoded = encode(state);
        const int previous = int(e->SendScintilla(QsciScintillaBase::SCI_GETLINESTATE, line));
        e->SendScintilla(QsciScintillaBase::SCI_SETLINESTATE, line, encoded);
        // Past the requested lines, go on only while this edit changes what
        // the following lines start with: lexer state or fold depth.
        if (line >= lastLine && encoded == previous && nextLevel >= 0)
            break;
    }
}

QColor SqlLexer::defaultColor(int style) const
{
    return color(style, m_dark);
}

QColor SqlLexer::color(int style, bool isDark)
{
    // Light and dark variants of a restrained palette.
    auto pick = [isDark](const char *light, const char *dark) {
        return QColor::fromString(QLatin1String(isDark ? dark : light));
    };
    switch (style) {
    case Comment:
        return pick("#6a737d", "#8b949e");
    case Keyword:
        return pick("#0033b3", "#79b8ff");
    case Type:
        return pick("#7a3e9d", "#d2a8ff");
    case QuotedIdentifier:
        return pick("#005c5c", "#56d4dd");
    case String:
        return pick("#067d17", "#7ee787");
    case Blob:
        return pick("#8a6d00", "#e3b341");
    case Number:
        return pick("#1750eb", "#79c0ff");
    case Operator:
        return pick("#871094", "#ff7b72");
    case Parameter:
        return pick("#b35900", "#ffa657");
    case DotCommand:
        return pick("#8a6d00", "#e3b341");
    case Unknown:
        return pick("#c62828", "#ff6b6b");
    default:
        return QGuiApplication::palette().color(QPalette::Text);
    }
}

QColor SqlLexer::defaultPaper(int) const
{
    return QGuiApplication::palette().color(QPalette::Base);
}

QFont SqlLexer::defaultFont(int style) const
{
    QFont font = m_font;
    if (style == Comment)
        font.setItalic(true);
    return font;
}

const char *SqlLexer::wordCharacters() const
{
    return "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_$";
}

} // namespace sqliteman
