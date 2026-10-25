// SPDX-FileCopyrightText: 2026 Petr Vanek
// SPDX-License-Identifier: GPL-3.0-or-later

#include "sql/Keywords.h"
#include "sql/Lexer.h"

#include <QTest>

using namespace sqliteman::sql;

namespace {

// "Kind:text" for each significant token, to compare in one go.
QByteArrayList lex(const QByteArray &text)
{
    static const char *const names[] = {"ws",  "comment", "kw",    "id",    "qid", "str",    "blob",
                                        "num", "op",      "punct", "param", "dot", "unknown"};
    QByteArrayList out;
    for (const Token &t : tokenize(text)) {
        if (t.kind == TokenKind::Whitespace)
            continue;
        out << QByteArray(names[int(t.kind)]) + ':' + text.mid(t.offset, t.length);
    }
    return out;
}

} // namespace

class TestLexer : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void tokens_data()
    {
        QTest::addColumn<QByteArray>("text");
        QTest::addColumn<QByteArrayList>("tokens");

        QTest::newRow("select") << QByteArray("SELECT a, 1 FROM t;")
                                << QByteArrayList {"kw:SELECT", "id:a", "punct:,", "num:1",
                                                   "kw:FROM",   "id:t", "punct:;"};
        QTest::newRow("keywords any case")
            << QByteArray("select Distinct") << QByteArrayList {"kw:select", "kw:Distinct"};
        QTest::newRow("quoted identifiers")
            << QByteArray("\"a\"\"b\" `c` [d e]")
            << QByteArrayList {"qid:\"a\"\"b\"", "qid:`c`", "qid:[d e]"};
        QTest::newRow("string with doubled quote")
            << QByteArray("'it''s'") << QByteArrayList {"str:'it''s'"};
        QTest::newRow("blob") << QByteArray("x'0AfF' X''")
                              << QByteArrayList {"blob:x'0AfF'", "blob:X''"};
        QTest::newRow("word starting with x") << QByteArray("xyz") << QByteArrayList {"id:xyz"};
        QTest::newRow("numbers") << QByteArray("1 2.5 .5 1e10 1.5E-3 0x1F 1_000")
                                 << QByteArrayList {"num:1",    "num:2.5",    "num:.5",
                                                    "num:1e10", "num:1.5E-3", "num:0x1F",
                                                    "num:1_000"};
        QTest::newRow("parameters")
            << QByteArray("? ?3 :name @at $dollar")
            << QByteArrayList {"param:?", "param:?3", "param::name", "param:@at", "param:$dollar"};
        QTest::newRow("operators")
            << QByteArray("a||b <> c->>'$.x' != d == e <= f")
            << QByteArrayList {"id:a",  "op:||", "id:b",  "op:<>", "id:c",  "op:->>", "str:'$.x'",
                               "op:!=", "id:d",  "op:==", "id:e",  "op:<=", "id:f"};
        QTest::newRow("comments") << QByteArray("a -- line\n/* block */ b")
                                  << QByteArrayList {"id:a", "comment:-- line",
                                                     "comment:/* block */", "id:b"};
        // SQLite's block comments do not nest.
        QTest::newRow("no nesting")
            << QByteArray("/* a /* b */ c */")
            << QByteArrayList {"comment:/* a /* b */", "id:c", "op:*", "op:/"};
        QTest::newRow("dollar in name") << QByteArray("a$b") << QByteArrayList {"id:a$b"};
        QTest::newRow("unicode name") << QByteArray("čaj") << QByteArrayList {"id:čaj"};
        QTest::newRow("dot command")
            << QByteArray(".tables\nSELECT 1;\n.mode csv")
            << QByteArrayList {"dot:.tables", "kw:SELECT", "num:1", "punct:;", "dot:.mode csv"};
        // Inside a statement, a dot at the start of a line is just a dot.
        QTest::newRow("dot inside statement")
            << QByteArray("SELECT t\n.a FROM t")
            << QByteArrayList {"kw:SELECT", "id:t", "punct:.", "id:a", "kw:FROM", "id:t"};
        QTest::newRow("unknown") << QByteArray("#") << QByteArrayList {"unknown:#"};
    }

    void tokens()
    {
        QFETCH(QByteArray, text);
        QFETCH(QByteArrayList, tokens);
        QCOMPARE(lex(text), tokens);
    }

    void resumesAcrossPieces()
    {
        // Line by line, as the editor's highlighter lexes.
        const QByteArray script = "SELECT '\nmulti\nline' /* a\ncomment */ \"q\nid\" [b\n]";
        LexState state;
        std::vector<Token> pieces;
        qsizetype at = 0;
        for (const QByteArray &line : script.split('\n')) {
            const QByteArray withEnd = line + '\n';
            for (Token t : tokenize(withEnd, state)) {
                t.offset += at;
                pieces.push_back(t);
            }
            at += withEnd.size();
        }
        QCOMPARE(state.mode, LexState::Mode::Code);
        QStringList kinds;
        for (const Token &t : pieces)
            if (t.kind != TokenKind::Whitespace)
                kinds << QString::number(int(t.kind));
        // The string, the comment and both identifiers are cut at each line
        // end, but keep their kind on every line.
        const QStringList expected {QString::number(int(TokenKind::Keyword)),
                                    QString::number(int(TokenKind::String)),
                                    QString::number(int(TokenKind::String)),
                                    QString::number(int(TokenKind::String)),
                                    QString::number(int(TokenKind::Comment)),
                                    QString::number(int(TokenKind::Comment)),
                                    QString::number(int(TokenKind::QuotedIdentifier)),
                                    QString::number(int(TokenKind::QuotedIdentifier)),
                                    QString::number(int(TokenKind::QuotedIdentifier)),
                                    QString::number(int(TokenKind::QuotedIdentifier))};
        QCOMPARE(kinds, expected);
    }

    void unterminatedLeavesState()
    {
        LexState state;
        tokenize("SELECT 'abc", state);
        QCOMPARE(state.mode, LexState::Mode::String);
        tokenize("def'", state);
        QCOMPARE(state.mode, LexState::Mode::Code);
        tokenize("[open", state);
        QCOMPARE(state.mode, LexState::Mode::QuotedIdentifier);
        QCOMPARE(state.close, ']');
    }

    void unquoting()
    {
        QCOMPARE(unquote("\"a\"\"b\""), QByteArray("a\"b"));
        QCOMPARE(unquote("'it''s'"), QByteArray("it's"));
        QCOMPARE(unquote("[x]]"), QByteArray("x]")); // ] cannot be escaped.
        QCOMPARE(unquote("`a``b`"), QByteArray("a`b"));
        QCOMPARE(unquote("plain"), QByteArray("plain"));
        QCOMPARE(unquote("x'00'"), QByteArray("00"));
    }

    void keywordsComeFromSqlite()
    {
        QVERIFY(isKeyword("select"));
        QVERIFY(isKeyword("RETURNING"));
        QVERIFY(!isKeyword("person"));
        const auto all = keywords();
        QVERIFY(all.size() > 100);
        QVERIFY(std::is_sorted(all.begin(), all.end()));
        QVERIFY(std::find(all.begin(), all.end(), QByteArray("WITHOUT")) != all.end());
    }

    void quotingNames()
    {
        QVERIFY(!needsQuoting("person"));
        QVERIFY(!needsQuoting("CamelCase")); // SQLite keeps the case as written.
        QVERIFY(needsQuoting("order"));
        QVERIFY(needsQuoting("two words"));
        QVERIFY(needsQuoting("1st"));
        QVERIFY(needsQuoting(""));
        QCOMPARE(identifier(QStringLiteral("person")), QStringLiteral("person"));
        QCOMPARE(identifier(QStringLiteral("my \"t\"")), QStringLiteral("\"my \"\"t\"\"\""));
    }
};

QTEST_GUILESS_MAIN(TestLexer)
#include "tst_lexer.moc"
