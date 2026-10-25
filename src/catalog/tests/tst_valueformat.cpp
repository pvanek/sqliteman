// SPDX-FileCopyrightText: 2026 Petr Vanek
// SPDX-License-Identifier: GPL-3.0-or-later

#include "catalog/ValueFormat.h"

#include <QByteArrayList>
#include <QTest>

using namespace sqliteman::catalog;

namespace {

// The tokens as "kind:text" pairs, to compare in one go.
QByteArrayList tokens(ValueSyntax syntax, QByteArrayView text)
{
    static const char *names[] = {"p", "key", "str", "num", "lit", "err"};
    QByteArrayList out;
    for (const ValueToken &t : tokenize(syntax, text))
        out << QByteArray(names[t.kind]) + ':' + text.sliced(t.start, t.length).toByteArray();
    return out;
}

// Formatted, or a marker that compares unequal to anything formatted.
QByteArray pj(QByteArrayView text)
{
    return prettyJson(text).value_or("<not JSON>");
}

} // namespace

class TestValueFormat : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void guessesFromTheValue()
    {
        QCOMPARE(guessSyntax("  {\"a\": [1, 2]} "), ValueSyntax::Json);
        QCOMPARE(guessSyntax("[true, null]"), ValueSyntax::Json);
        QCOMPARE(guessSyntax("{a,b}"), ValueSyntax::Text);
        QCOMPARE(guessSyntax("plain words"), ValueSyntax::Text);
        QCOMPARE(guessSyntax("42"), ValueSyntax::Text);
        QCOMPARE(guessSyntax(""), ValueSyntax::Text);
    }

    void tokenizesJson()
    {
        QCOMPARE(
            tokens(ValueSyntax::Json, "{\"a\": [1, -2.5e3, \"x\\\"y\"], \"b\" :true, \"c\":null}"),
            QByteArrayList({"p:{", "key:\"a\"", "p::", "p:[", "num:1", "p:,", "num:-2.5e3", "p:,",
                            "str:\"x\\\"y\"", "p:]", "p:,", "key:\"b\"", "p::", "lit:true", "p:,",
                            "key:\"c\"", "p::", "lit:null", "p:}"}));
        // What is not JSON is marked, and the rest still tokenized.
        QCOMPARE(tokens(ValueSyntax::Json, "[01, True, \"open"),
                 QByteArrayList({"p:[", "err:01", "p:,", "err:True", "p:,", "err:\"open"}));
    }

    void validatesJson()
    {
        QVERIFY(isValidJson("{\"a\": {\"b\": [1, {}, []]}}"));
        QVERIFY(isValidJson("\"just a string\""));
        QVERIFY(isValidJson(" 12 "));
        QVERIFY(!isValidJson("{1, 2}"));
        QVERIFY(!isValidJson("{\"a\" 1}"));
        QVERIFY(!isValidJson("[1, 2,]"));
        QVERIFY(!isValidJson("[1] [2]"));
        QVERIFY(!isValidJson("[1"));
        QVERIFY(!isValidJson(""));
        // Deeper than the parser goes: shown as it is rather than parsed.
        QVERIFY(!isValidJson(QByteArray(5000, '[') + QByteArray(5000, ']')));
    }

    void prettyJsonKeepsOrderAndNumbers()
    {
        // The members stay in their order and numbers as written; a round
        // trip through a double would turn the big one into 1e+20.
        const auto formatted
            = prettyJson("{\"z\":1,\"a\":[100000000000000000001,{},[]],\"m\":{\"k\":\"ü\"}}");
        QVERIFY(formatted);
        QCOMPARE(*formatted,
                 QByteArray("{\n"
                            "    \"z\": 1,\n"
                            "    \"a\": [\n"
                            "        100000000000000000001,\n"
                            "        {},\n"
                            "        []\n"
                            "    ],\n"
                            "    \"m\": {\n"
                            "        \"k\": \"ü\"\n"
                            "    }\n"
                            "}"));
        QCOMPARE(pj("[]"), QByteArray("[]"));
        QCOMPARE(pj("\"a,b:{c}\""), QByteArray("\"a,b:{c}\""));
        QVERIFY(!prettyJson("{oops}"));
    }

    void textHasNoTokens()
    {
        QVERIFY(tokens(ValueSyntax::Text, "{\"a\": 1}").isEmpty());
        QVERIFY(!pretty(ValueSyntax::Text, "{\"a\": 1}"));
    }
};

QTEST_GUILESS_MAIN(TestValueFormat)
#include "tst_valueformat.moc"
