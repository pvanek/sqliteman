// SPDX-FileCopyrightText: 2026 Petr Vanek
// SPDX-License-Identifier: GPL-3.0-or-later

#include "sql/Splitter.h"

#include <QTest>

#include <sqlite3.h>

using sqliteman::sql::splitStatements;
using sqliteman::sql::statementAt;
using sqliteman::sql::StatementSpan;

namespace {

QByteArrayList texts(const QByteArray &script)
{
    QByteArrayList out;
    for (const StatementSpan &s : splitStatements(script))
        out << script.mid(s.offset, s.length);
    return out;
}

} // namespace

class TestSplitter : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void split_data()
    {
        QTest::addColumn<QByteArray>("script");
        QTest::addColumn<QByteArrayList>("statements");

        QTest::newRow("empty") << QByteArray() << QByteArrayList {};
        QTest::newRow("whitespace only") << QByteArray(" \n\t\r\n ") << QByteArrayList {};
        QTest::newRow("comments only") << QByteArray("-- a\n/* b */\n") << QByteArrayList {};
        QTest::newRow("empty statements") << QByteArray(";;\n;") << QByteArrayList {};
        QTest::newRow("simple") << QByteArray("SELECT 1; SELECT 2;")
                                << QByteArrayList {"SELECT 1", "SELECT 2"};
        QTest::newRow("no trailing semicolon")
            << QByteArray("SELECT 1; SELECT 2") << QByteArrayList {"SELECT 1", "SELECT 2"};
        QTest::newRow("multi-line")
            << QByteArray("SELECT\n  a\nFROM t;\n") << QByteArrayList {"SELECT\n  a\nFROM t"};
        QTest::newRow("leading comment excluded")
            << QByteArray("-- note\nSELECT 1;") << QByteArrayList {"SELECT 1"};
        QTest::newRow("inner comment kept")
            << QByteArray("SELECT 1 -- one\n + 2;") << QByteArrayList {"SELECT 1 -- one\n + 2"};
        QTest::newRow("semicolon in comment") << QByteArray("SELECT 1 /* ; */; SELECT 2 -- ;\n;")
                                              << QByteArrayList {"SELECT 1", "SELECT 2"};
        QTest::newRow("semicolon in string") << QByteArray("SELECT 'a;b'; SELECT 'it''s;';")
                                             << QByteArrayList {"SELECT 'a;b'", "SELECT 'it''s;'"};
        QTest::newRow("semicolon in identifiers")
            << QByteArray("SELECT \"a;b\", `c;d`, [e;f]; SELECT 2")
            << QByteArrayList {"SELECT \"a;b\", `c;d`, [e;f]", "SELECT 2"};
        QTest::newRow("trigger") << QByteArray(
            "CREATE TRIGGER tr AFTER INSERT ON t BEGIN\n"
            "  UPDATE t SET a = 1;\n  DELETE FROM u;\nEND;\nSELECT 2;")
                                 << QByteArrayList {"CREATE TRIGGER tr AFTER INSERT ON t BEGIN\n"
                                                    "  UPDATE t SET a = 1;\n  DELETE FROM u;\nEND",
                                                    "SELECT 2"};
        QTest::newRow("temp trigger, lower case")
            << QByteArray("create temp trigger tr after delete on t begin select 1; end; select 2")
            << QByteArrayList {"create temp trigger tr after delete on t begin select 1; end",
                               "select 2"};
        // CASE ... END inside a trigger never follows a semicolon.
        QTest::newRow("CASE in trigger")
            << QByteArray("CREATE TRIGGER tr BEFORE UPDATE ON t BEGIN SELECT CASE WHEN 1 THEN 2 "
                          "END; END; SELECT 3")
            << QByteArrayList {"CREATE TRIGGER tr BEFORE UPDATE ON t BEGIN SELECT CASE WHEN 1 "
                               "THEN 2 END; END",
                               "SELECT 3"};
        QTest::newRow("CASE outside a trigger")
            << QByteArray("SELECT CASE WHEN 1 THEN 2 END; SELECT 3")
            << QByteArrayList {"SELECT CASE WHEN 1 THEN 2 END", "SELECT 3"};
        QTest::newRow("explain create trigger")
            << QByteArray("EXPLAIN CREATE TRIGGER tr AFTER INSERT ON t BEGIN SELECT 1; END; X")
            << QByteArrayList {"EXPLAIN CREATE TRIGGER tr AFTER INSERT ON t BEGIN SELECT 1; END",
                               "X"};
        QTest::newRow("create table is not a trigger")
            << QByteArray("CREATE TABLE trigger_log(a); SELECT 1")
            << QByteArrayList {"CREATE TABLE trigger_log(a)", "SELECT 1"};
        QTest::newRow("transaction block")
            << QByteArray("BEGIN; INSERT INTO t VALUES (1); END;")
            << QByteArrayList {"BEGIN", "INSERT INTO t VALUES (1)", "END"};
        QTest::newRow("dot commands") << QByteArray(".headers on\nSELECT 1;\n.mode csv\n")
                                      << QByteArrayList {".headers on", "SELECT 1", ".mode csv"};
        QTest::newRow("unterminated string runs to the end")
            << QByteArray("SELECT 'abc; SELECT 2") << QByteArrayList {"SELECT 'abc; SELECT 2"};
        QTest::newRow("unterminated comment hides the rest")
            << QByteArray("SELECT 1; /* SELECT 2;") << QByteArrayList {"SELECT 1"};
        QTest::newRow("unterminated trigger runs to the end")
            << QByteArray("CREATE TRIGGER tr AFTER INSERT ON t BEGIN SELECT 1;")
            << QByteArrayList {"CREATE TRIGGER tr AFTER INSERT ON t BEGIN SELECT 1"};
    }

    void split()
    {
        QFETCH(QByteArray, script);
        QFETCH(QByteArrayList, statements);
        QCOMPARE(texts(script), statements);
    }

    void terminated()
    {
        auto spans = splitStatements("SELECT 1; SELECT 2");
        QCOMPARE(spans.size(), 2u);
        QVERIFY(spans[0].terminated);
        QVERIFY(!spans[1].terminated);
        spans = splitStatements("CREATE TRIGGER tr AFTER INSERT ON t BEGIN SELECT 1;");
        QVERIFY(!spans[0].terminated);
        spans = splitStatements(".tables");
        QCOMPARE(spans[0].kind, StatementSpan::Kind::DotCommand);
    }

    void agreesWithSqliteComplete_data()
    {
        QTest::addColumn<QByteArray>("statement");
        QTest::newRow("plain") << QByteArray("SELECT 1;");
        QTest::newRow("open string") << QByteArray("SELECT 'a;");
        QTest::newRow("trigger body")
            << QByteArray("CREATE TRIGGER x AFTER INSERT ON t BEGIN SELECT 1;");
        QTest::newRow("trigger done")
            << QByteArray("CREATE TRIGGER x AFTER INSERT ON t BEGIN SELECT 1; END;");
        QTest::newRow("case in trigger") << QByteArray(
            "CREATE TRIGGER x AFTER INSERT ON t BEGIN SELECT CASE 1 WHEN 1 THEN 1 END;");
        QTest::newRow("comment") << QByteArray("SELECT 1 -- ;");
    }

    void agreesWithSqliteComplete()
    {
        // A statement is complete for SQLite exactly when it is a single
        // terminated span here.
        QFETCH(QByteArray, statement);
        const auto spans = splitStatements(statement);
        const bool complete = spans.size() == 1 && spans[0].terminated;
        QCOMPARE(complete, sqlite3_complete(statement.constData()) != 0);
    }

    void statementUnderCursor()
    {
        const QByteArray script = "SELECT 1;\n\nSELECT 2; SELECT 3;\n";
        const auto spans = splitStatements(script);
        QCOMPARE(statementAt(script, spans, 0), 0);
        QCOMPARE(statementAt(script, spans, 8), 0); // On the semicolon.
        QCOMPARE(statementAt(script, spans, 10), -1); // The empty line.
        QCOMPARE(statementAt(script, spans, script.indexOf("SELECT 3") + 3), 2);
        // At the end of a line: the statement before the cursor.
        QCOMPARE(statementAt(script, spans, script.size() - 1), 2);
    }

    void offsetsAreUtf8Bytes()
    {
        const QByteArray script = "SELECT 'ěščř'; SELECT 2";
        const auto spans = splitStatements(script);
        QCOMPARE(spans.size(), 2u);
        QCOMPARE(script.mid(spans[1].offset, spans[1].length), QByteArray("SELECT 2"));
    }
};

QTEST_GUILESS_MAIN(TestSplitter)
#include "tst_splitter.moc"
