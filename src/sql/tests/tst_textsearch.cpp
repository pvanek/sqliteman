// SPDX-FileCopyrightText: 2026 Petr Vanek
// SPDX-License-Identifier: GPL-3.0-or-later

#include "sql/TextSearch.h"

#include <QTest>

using namespace sqliteman::sql;
using Mode = TextSearch::Mode;

namespace {

// Matches as "from-to" pairs, which QCOMPARE prints readably.
QStringList ranges(const std::vector<TextSearch::Match> &matches)
{
    QStringList out;
    for (const TextSearch::Match &m : matches)
        out << QStringLiteral("%1-%2").arg(m.from).arg(m.to);
    return out;
}

QStringList all(const QByteArray &text, const QString &pattern, Mode mode, bool matchCase = false)
{
    return ranges(TextSearch(pattern, mode, matchCase).findAll(text));
}

QByteArray replaceAll(const QByteArray &text, const QString &pattern, Mode mode,
                      const QString &with, const QString &newline = QStringLiteral("\n"))
{
    const TextSearch search(pattern, mode, false);
    QByteArray out = text;
    const auto matches = search.findAll(text);
    for (auto it = matches.rbegin(); it != matches.rend(); ++it)
        out.replace(it->from, it->to - it->from, search.replacement(*it, with, newline));
    return out;
}

} // namespace

class TestTextSearch : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void plainText()
    {
        QCOMPARE(all("SELECT id FROM t WHERE ID = 1", QStringLiteral("id"), Mode::PlainText),
                 (QStringList {QStringLiteral("7-9"), QStringLiteral("23-25")}));
        QCOMPARE(all("SELECT id FROM t WHERE ID = 1", QStringLiteral("id"), Mode::PlainText, true),
                 QStringList {QStringLiteral("7-9")});
        // Characters a regular expression would read are only themselves.
        QCOMPARE(all("a.b axb (a.b)", QStringLiteral("(a.b)"), Mode::PlainText),
                 QStringList {QStringLiteral("8-13")});
        // An empty pattern finds nothing, and is no error.
        const TextSearch empty(QString(), Mode::PlainText, false);
        QVERIFY(!empty.isValid());
        QVERIFY(empty.errorString().isEmpty());
        QVERIFY(empty.findAll("abc").empty());
    }

    void wholeWords()
    {
        const QByteArray text = "id, user_id, id2, \"id\" ,id";
        QCOMPARE(all(text, QStringLiteral("id"), Mode::WholeWords),
                 (QStringList {QStringLiteral("0-2"), QStringLiteral("19-21"),
                               QStringLiteral("24-26")}));
        // A pattern ending in something other than a letter is a word too.
        QCOMPARE(all("x = 1, xx = 2", QStringLiteral("x ="), Mode::WholeWords),
                 QStringList {QStringLiteral("0-3")});
        // Letters beyond ASCII are word characters.
        QCOMPARE(all("cena, čcena", QStringLiteral("cena"), Mode::WholeWords),
                 QStringList {QStringLiteral("0-4")});
    }

    void escapeSequences()
    {
        QCOMPARE(all("a\tb a\\tb", QStringLiteral("a\\tb"), Mode::EscapeSequences),
                 QStringList {QStringLiteral("0-3")});
        QCOMPARE(all("a\\b", QStringLiteral("a\\\\b"), Mode::EscapeSequences),
                 QStringList {QStringLiteral("0-3")});
        // \n stands for whichever line break the file has.
        QCOMPARE(all("x;\ny;\r\nz", QStringLiteral(";\\n"), Mode::EscapeSequences),
                 (QStringList {QStringLiteral("1-3"), QStringLiteral("4-7")}));
        // Anything else is plain text, regular expression characters included.
        QCOMPARE(all("a.b \\d", QStringLiteral("\\d"), Mode::EscapeSequences),
                 QStringList {QStringLiteral("4-6")});
    }

    void regularExpression()
    {
        QCOMPARE(all("t1 t22 x3", QStringLiteral("t\\d+"), Mode::RegularExpression),
                 (QStringList {QStringLiteral("0-2"), QStringLiteral("3-6")}));
        // ^ and $ are line boundaries, with either line break.
        QCOMPARE(all("a;\r\nb;\nc", QStringLiteral(";$"), Mode::RegularExpression),
                 (QStringList {QStringLiteral("1-2"), QStringLiteral("5-6")}));
        QCOMPARE(all("a\nb", QStringLiteral("^"), Mode::RegularExpression),
                 (QStringList {QStringLiteral("0-0"), QStringLiteral("2-2")}));

        const TextSearch broken(QStringLiteral("(a"), Mode::RegularExpression, false);
        QVERIFY(!broken.isValid());
        QVERIFY(!broken.errorString().isEmpty());
        QVERIFY(broken.findAll("(a").empty());
    }

    // Positions are bytes of UTF-8 text, as in the editor.
    void bytePositions()
    {
        const QByteArray text = QStringLiteral("SELECT 'žluťoučký 🐘' AS kůň").toUtf8();
        QCOMPARE(
            all(text, QStringLiteral("kůň"), Mode::PlainText),
            QStringList {QStringLiteral("%1-%2").arg(text.indexOf("k\xc5\xaf")).arg(text.size())});
        const auto elephant
            = TextSearch(QStringLiteral("🐘"), Mode::PlainText, false).findForward(text, 0, false);
        QVERIFY(elephant);
        QCOMPARE(text.sliced(elephant->from, elephant->to - elephant->from),
                 QStringLiteral("🐘").toUtf8());
    }

    void forwardAndBackward()
    {
        const QByteArray text = "ab ab ab";
        const TextSearch search(QStringLiteral("ab"), Mode::PlainText, false);
        QCOMPARE(search.findForward(text, 0, false)->from, 0);
        QCOMPARE(search.findForward(text, 1, false)->from, 3);
        QVERIFY(!search.findForward(text, 7, false));
        QCOMPARE(search.findBackward(text, 6)->from, 3);
        QCOMPARE(search.findBackward(text, 7)->from, 6);
        QVERIFY(!search.findBackward(text, 0));

        // Only within a range, as when searching in the selection.
        QCOMPARE(search.findForward(text, 0, false, 2, 6)->from, 3);
        QVERIFY(!search.findForward(text, 4, false, 2, 6));
        QCOMPARE(search.findBackward(text, 8, 2, 6)->from, 3);
        QCOMPARE(ranges(search.findAll(text, 2, 6)), QStringList {QStringLiteral("3-5")});

        // An empty match where the last one was is passed over, or search
        // would stand still.
        const TextSearch lineStart(QStringLiteral("^"), Mode::RegularExpression, false);
        QCOMPARE(lineStart.findForward("a\nb", 0, false)->from, 0);
        QCOMPARE(lineStart.findForward("a\nb", 0, true)->from, 2);
        QVERIFY(!lineStart.findForward("a\nb", 2, true));
    }

    // A lookbehind sees the text before the range it searches.
    void contextBeforeRange()
    {
        const TextSearch search(QStringLiteral("(?<=a)b"), Mode::RegularExpression, false);
        QCOMPARE(ranges(search.findAll("ab cb", 1)), QStringList {QStringLiteral("1-2")});
    }

    void replacement()
    {
        // Plain text is taken as it is, backslashes and all.
        QCOMPARE(replaceAll("a a", QStringLiteral("a"), Mode::PlainText, QStringLiteral("\\1")),
                 QByteArray("\\1 \\1"));
        QCOMPARE(replaceAll("a;b;", QStringLiteral(";"), Mode::EscapeSequences,
                            QStringLiteral(";\\n\\t")),
                 QByteArray("a;\n\tb;\n\t"));
        // Captures, and a line break the way the file writes it.
        QCOMPARE(replaceAll("x = 1, y = 2", QStringLiteral("(\\w) = (\\d)"),
                            Mode::RegularExpression, QStringLiteral("\\2 AS \\1"),
                            QStringLiteral("\r\n")),
                 QByteArray("1 AS x, 2 AS y"));
        QCOMPARE(replaceAll("a,b", QStringLiteral(","), Mode::RegularExpression,
                            QStringLiteral(",\\n"), QStringLiteral("\r\n")),
                 QByteArray("a,\r\nb"));
        // Commenting out every line by replacing ^.
        QCOMPARE(
            replaceAll("a\nb", QStringLiteral("^"), Mode::RegularExpression, QStringLiteral("-- ")),
            QByteArray("-- a\n-- b"));
        // Case changes.
        QCOMPARE(replaceAll("select from", QStringLiteral("(\\w+) (\\w+)"), Mode::RegularExpression,
                            QStringLiteral("\\U\\1\\E \\u\\2")),
                 QByteArray("SELECT From"));
        QCOMPARE(replaceAll("ABC", QStringLiteral("\\w+"), Mode::RegularExpression,
                            QStringLiteral("\\L\\0")),
                 QByteArray("abc"));
        // A capture that is not there is nothing; an unknown escape stays.
        QCOMPARE(replaceAll("a", QStringLiteral("a"), Mode::RegularExpression,
                            QStringLiteral("[\\5]\\q\\\\")),
                 QByteArray("[]\\q\\"));
    }
};

QTEST_GUILESS_MAIN(TestTextSearch)
#include "tst_textsearch.moc"
