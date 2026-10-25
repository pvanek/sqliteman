// SPDX-FileCopyrightText: 2026 Petr Vanek
// SPDX-License-Identifier: GPL-3.0-or-later

#include "TextSearch.h"

#include <algorithm>

namespace sqliteman::sql {

namespace {

// The UTF-16 index of a byte offset in UTF-8 text.
qsizetype utf16Index(const QByteArray &utf8, qsizetype bytes)
{
    qsizetype units = 0;
    bytes = std::min(bytes, utf8.size());
    for (qsizetype i = 0; i < bytes; ++i) {
        const auto b = uchar(utf8[i]);
        if ((b & 0xC0) != 0x80) // Not a continuation byte.
            units += b >= 0xF0 ? 2 : 1; // Four bytes make a surrogate pair.
    }
    return units;
}

// Byte offsets of UTF-16 indexes, which must not go down from one call to
// the next: one pass over the text for all the matches.
class ByteOffsets
{
public:
    explicit ByteOffsets(const QString &text) : m_text(text) { }

    qsizetype at(qsizetype index)
    {
        for (; m_index < index; ++m_index) {
            const char16_t c = m_text[m_index].unicode();
            m_bytes += c < 0x80 ? 1 : c < 0x800 ? 2 : QChar::isSurrogate(c) ? 2 : 3;
        }
        return m_bytes;
    }

private:
    const QString &m_text;
    qsizetype m_index = 0;
    qsizetype m_bytes = 0;
};

// The pattern of escape sequence mode as a regular expression: literal text
// but for the sequences. A line break matches whichever the file uses.
QString unescapedPattern(const QString &pattern)
{
    QString out;
    QString literal;
    auto flush = [&] {
        out += QRegularExpression::escape(literal);
        literal.clear();
    };
    for (qsizetype i = 0; i < pattern.size(); ++i) {
        const QChar c = pattern[i];
        if (c != QLatin1Char('\\') || i + 1 == pattern.size()) {
            literal += c;
            continue;
        }
        const QChar next = pattern[++i];
        if (next == QLatin1Char('n')) {
            flush();
            out += QStringLiteral("\\R");
        } else if (next == QLatin1Char('t')) {
            literal += QLatin1Char('\t');
        } else if (next == QLatin1Char('r')) {
            literal += QLatin1Char('\r');
        } else if (next == QLatin1Char('\\')) {
            literal += QLatin1Char('\\');
        } else {
            literal += c;
            literal += next;
        }
    }
    flush();
    return out;
}

} // namespace

TextSearch::TextSearch(const QString &pattern, Mode mode, bool matchCase)
    : m_pattern(pattern), m_mode(mode), m_matchCase(matchCase)
{
    QString regex;
    switch (mode) {
    case Mode::PlainText:
        regex = QRegularExpression::escape(pattern);
        break;
    case Mode::WholeWords:
        // Lookarounds rather than \b, which would ask for a word character
        // at each end of the pattern: "id =" is a whole word in "id = 1".
        regex = QStringLiteral("(?<!\\w)") + QRegularExpression::escape(pattern)
            + QStringLiteral("(?!\\w)");
        break;
    case Mode::EscapeSequences:
        regex = unescapedPattern(pattern);
        break;
    case Mode::RegularExpression:
        // Lines end in \r\n too, so that $ comes before it and . stops there.
        regex = QStringLiteral("(*ANYCRLF)") + pattern;
        break;
    }
    QRegularExpression::PatternOptions options = QRegularExpression::UseUnicodePropertiesOption;
    if (mode == Mode::RegularExpression)
        options |= QRegularExpression::MultilineOption;
    if (!matchCase)
        options |= QRegularExpression::CaseInsensitiveOption;
    m_regex.setPattern(regex);
    m_regex.setPatternOptions(options);
}

bool TextSearch::isValid() const
{
    return !m_pattern.isEmpty() && m_regex.isValid();
}

QString TextSearch::errorString() const
{
    return m_regex.isValid() ? QString() : m_regex.errorString();
}

std::vector<TextSearch::Match> TextSearch::findAll(const QByteArray &text, qsizetype from,
                                                   qsizetype to) const
{
    std::vector<Match> out;
    if (!isValid())
        return out;
    const QString text16 = QString::fromUtf8(text);
    if (to < 0)
        to = text.size();
    ByteOffsets bytes(text16);
    auto it = m_regex.globalMatch(text16, utf16Index(text, from));
    const qsizetype end16 = utf16Index(text, to);
    while (it.hasNext()) {
        const QRegularExpressionMatch m = it.next();
        if (m.capturedEnd() > end16)
            break;
        Match match;
        match.from = bytes.at(m.capturedStart());
        match.to = bytes.at(m.capturedEnd());
        if (m_mode == Mode::RegularExpression)
            match.captures = m.capturedTexts();
        out.push_back(std::move(match));
    }
    return out;
}

std::optional<TextSearch::Match> TextSearch::findForward(const QByteArray &text, qsizetype pos,
                                                         bool skipEmptyAt, qsizetype from,
                                                         qsizetype to) const
{
    if (!isValid())
        return std::nullopt;
    const QString text16 = QString::fromUtf8(text);
    if (to < 0)
        to = text.size();
    const qsizetype pos16 = utf16Index(text, std::max(pos, from));
    const qsizetype end16 = utf16Index(text, to);
    auto it = m_regex.globalMatch(text16, pos16);
    while (it.hasNext()) {
        const QRegularExpressionMatch m = it.next();
        if (m.capturedEnd() > end16)
            break;
        if (skipEmptyAt && m.capturedLength() == 0 && m.capturedStart() == pos16)
            continue;
        ByteOffsets bytes(text16);
        Match match;
        match.from = bytes.at(m.capturedStart());
        match.to = bytes.at(m.capturedEnd());
        if (m_mode == Mode::RegularExpression)
            match.captures = m.capturedTexts();
        return match;
    }
    return std::nullopt;
}

std::optional<TextSearch::Match> TextSearch::findBackward(const QByteArray &text, qsizetype pos,
                                                          qsizetype from, qsizetype to) const
{
    if (!isValid())
        return std::nullopt;
    if (to < 0)
        to = text.size();
    // Regular expressions only run forwards: the last of the matches
    // before pos is the one.
    std::optional<Match> last;
    for (Match &m : findAll(text, from, to)) {
        if (m.from >= pos)
            break;
        last = std::move(m);
    }
    return last;
}

QByteArray TextSearch::replacement(const Match &match, const QString &with,
                                   const QString &newline) const
{
    if (m_mode == Mode::PlainText || m_mode == Mode::WholeWords)
        return with.toUtf8();

    const bool regex = m_mode == Mode::RegularExpression;
    enum class Case { Keep, Upper, Lower };
    Case all = Case::Keep; // From \U or \L until \E.
    Case next = Case::Keep; // From \u or \l, for one character.
    QString out;
    auto append = [&](const QString &s) {
        for (const QChar c : s) {
            const Case apply = next != Case::Keep ? next : all;
            next = Case::Keep;
            out += apply == Case::Upper ? c.toUpper() : apply == Case::Lower ? c.toLower() : c;
        }
    };
    for (qsizetype i = 0; i < with.size(); ++i) {
        const QChar c = with[i];
        if (c != QLatin1Char('\\') || i + 1 == with.size()) {
            append(c);
            continue;
        }
        const QChar e = with[++i];
        if (e == QLatin1Char('n')) {
            append(newline);
        } else if (e == QLatin1Char('t')) {
            append(QStringLiteral("\t"));
        } else if (e == QLatin1Char('r')) {
            append(QStringLiteral("\r"));
        } else if (e == QLatin1Char('\\')) {
            append(QStringLiteral("\\"));
        } else if (regex && e.isDigit()) {
            const int n = e.digitValue();
            append(n < match.captures.size() ? match.captures[n] : QString());
        } else if (regex && e == QLatin1Char('U')) {
            all = Case::Upper;
        } else if (regex && e == QLatin1Char('L')) {
            all = Case::Lower;
        } else if (regex && e == QLatin1Char('E')) {
            all = Case::Keep;
        } else if (regex && e == QLatin1Char('u')) {
            next = Case::Upper;
        } else if (regex && e == QLatin1Char('l')) {
            next = Case::Lower;
        } else {
            append(QString(c) + e);
        }
    }
    return out.toUtf8();
}

} // namespace sqliteman::sql
