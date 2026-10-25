// SPDX-FileCopyrightText: 2026 Petr Vanek
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "sql/Lexer.h"

#include "Qsci.h"

#include <QFont>

namespace sqliteman {

// Syntax highlighting for SQLite's SQL: styles QScintilla text with
// sql::tokenize(), line by line. Each line's end state is kept in Scintilla's
// per-line state, so an edit restyles from its line on, and only as far as
// the state it leaves behind differs from before.
class SqlLexer : public QsciLexerCustom
{
    Q_OBJECT

public:
    enum Style {
        Default,
        Comment,
        Keyword,
        Type,
        Identifier,
        QuotedIdentifier,
        String,
        Blob,
        Number,
        Operator,
        Punctuation,
        Parameter,
        DotCommand,
        Unknown,
        StyleCount,
    };

    explicit SqlLexer(QObject *parent = nullptr);

    const char *language() const override { return "SQLite"; }
    QString description(int style) const override;
    void styleText(int start, int end) override;

    QColor defaultColor(int style) const override;
    QColor defaultPaper(int style) const override;
    QFont defaultFont(int style) const override;
    const char *wordCharacters() const override;

    // The font every style starts from, the preferences' editor font.
    void setBaseFont(const QFont &font) { m_font = font; }

    static Style styleFor(const sql::Token &token, QByteArrayView text);
    static QColor color(int style, bool dark);
    bool isDark() const { return m_dark; }
    // Re-reads the application palette, e.g. after a theme change.
    void refreshPalette();

    // Folding. A line's fold level is its depth at the start of the line:
    // a comment or string the lexer state leaves open, plus structure:
    // statements spanning lines, parentheses, CASE ... END and the BEGIN ...
    // END of a trigger.
    static int stateDepth(const sql::LexState &state);
    static int foldDepthAfter(const std::vector<sql::Token> &tokens, QByteArrayView text,
                              int depth);

    // Packing a lexer state into Scintilla's per-line int, for tests.
    static int encode(const sql::LexState &state);
    static sql::LexState decode(int value);

private:
    bool m_dark = false;
    QFont m_font;
};

} // namespace sqliteman
