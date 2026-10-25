// SPDX-FileCopyrightText: 2026 Petr Vanek
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QByteArray>
#include <QRegularExpression>
#include <QString>
#include <QStringList>

#include <optional>
#include <vector>

namespace sqliteman::sql {

// Find and replace in a script, the way Kate's search bar does it: plain
// text, whole words, text with escape sequences, or a regular expression.
// Text and positions are UTF-8 and in bytes, as in the editor.
class TextSearch
{
public:
    enum class Mode {
        PlainText,
        WholeWords, // Plain text not inside a longer word.
        EscapeSequences, // Plain text where \n, \t, \r and \\ mean what they do in C.
        RegularExpression,
    };

    struct Match
    {
        qsizetype from = 0;
        qsizetype to = 0;
        QStringList captures; // With a regular expression: \0, \1 and on.
    };

    TextSearch() = default;
    TextSearch(const QString &pattern, Mode mode, bool matchCase);

    QString pattern() const { return m_pattern; }
    Mode mode() const { return m_mode; }
    bool matchCase() const { return m_matchCase; }
    // An empty pattern, or a regular expression with an error, finds nothing.
    bool isValid() const;
    QString errorString() const;

    // Every match within bytes [from, to) (to < 0: to the end), in order and
    // not overlapping. Empty matches count: replacing ^ adds to every line.
    std::vector<Match> findAll(const QByteArray &text, qsizetype from = 0, qsizetype to = -1) const;
    // The first match starting at pos or after it, within [from, to). An
    // empty match at pos itself is skipped when skipEmptyAt is set, so
    // searching on from the last match moves on.
    std::optional<Match> findForward(const QByteArray &text, qsizetype pos, bool skipEmptyAt,
                                     qsizetype from = 0, qsizetype to = -1) const;
    // The last match starting before pos, within [from, to).
    std::optional<Match> findBackward(const QByteArray &text, qsizetype pos, qsizetype from = 0,
                                      qsizetype to = -1) const;

    // What replaces a match. With a regular expression, \0 to \9 are its
    // captures, \U, \L and \E upper- and lowercase what follows until \E,
    // \u and \l the next character; with regular expressions and escape
    // sequences, \n is a line break written as newline, \t a tab and \\ a
    // backslash. Plain text and whole words take it as it is.
    QByteArray replacement(const Match &match, const QString &with,
                           const QString &newline = QStringLiteral("\n")) const;

private:
    QString m_pattern;
    Mode m_mode = Mode::PlainText;
    bool m_matchCase = false;
    QRegularExpression m_regex;
};

} // namespace sqliteman::sql
