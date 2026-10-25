// SPDX-FileCopyrightText: 2026 Petr Vanek
// SPDX-License-Identifier: GPL-3.0-or-later

#include "sqlite/Connection.h"
#include "sqlite/RowStore.h"

#include <QFile>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

#include <sqlite3.h>

using sqliteman::sqlite::Connection;
using sqliteman::sqlite::ExecuteOptions;
using sqliteman::sqlite::OpenOptions;
using sqliteman::sqlite::Result;
using sqliteman::sqlite::RowStore;
using sqliteman::sqlite::TaskOutcome;
using sqliteman::sqlite::Value;
using sqliteman::sqlite::ValueType;
using State = Connection::State;

namespace {

// Runs sql and collects what comes back until the query is done.
std::vector<Result> run(Connection &c, const QByteArray &sql, const ExecuteOptions &options = {})
{
    std::vector<Result> results;
    QObject context;
    QObject::connect(&c, &Connection::resultReady, &context,
                     [&](const Result &r) { results.push_back(r); });
    QSignalSpy finished(&c, &Connection::queryFinished);
    if (!c.execute(sql, options))
        return {};
    if (!finished.wait(10'000))
        return {};
    return results;
}

} // namespace

class TestConnection : public QObject
{
    Q_OBJECT

private:
    QTemporaryDir m_dir;
    QString path(const QString &name) const { return m_dir.filePath(name); }

    void openNew(Connection &c, const QString &name)
    {
        OpenOptions options;
        options.create = true;
        c.open(path(name), options);
        QTRY_COMPARE(c.state(), State::Ready);
    }

private Q_SLOTS:
    void opensAndCreates()
    {
        Connection c;
        OpenOptions options;
        options.create = true;
        c.open(path(QStringLiteral("new.db")), options);
        QCOMPARE(c.state(), State::Opening);
        QTRY_COMPARE(c.state(), State::Ready);
        QVERIFY(!c.isReadOnly());
        QVERIFY(!c.inTransaction());
    }

    void missingFileIsNotCreatedUnlessAsked()
    {
        Connection c;
        c.open(path(QStringLiteral("missing.db")));
        QTRY_COMPARE(c.state(), State::Failed);
        QVERIFY(!c.errorMessage().isEmpty());
        QVERIFY(!QFile::exists(path(QStringLiteral("missing.db"))));
    }

    void fileThatIsNotADatabaseFails()
    {
        // Opening alone would succeed: SQLite reads the file only when asked.
        QFile file(path(QStringLiteral("text.db")));
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write(QByteArray(4096, 'x'));
        file.close();
        Connection c;
        c.open(file.fileName());
        QTRY_COMPARE(c.state(), State::Failed);
        QVERIFY2(c.errorMessage().contains(QLatin1String("not a database")),
                 qPrintable(c.errorMessage()));
    }

    void walDatabaseOpens()
    {
        // Issue #18: sqliteman 1.2 could not open a database in WAL mode.
        {
            Connection c;
            openNew(c, QStringLiteral("wal.db"));
            QVERIFY(
                !run(c, "PRAGMA journal_mode = WAL; CREATE TABLE t(x); INSERT INTO t VALUES (1)")
                     .back()
                     .isError());
        }
        Connection c;
        c.open(path(QStringLiteral("wal.db")));
        QTRY_COMPARE(c.state(), State::Ready);
        const auto results = run(c, "SELECT x FROM t");
        QCOMPARE(results.size(), 1u);
        QCOMPARE(results[0].value(0, 0), QByteArrayView("1"));
    }

    void valuesKeepTheirStorageClass()
    {
        Connection c;
        openNew(c, QStringLiteral("values.db"));
        const auto results = run(c, "SELECT NULL, 42, 0.1 + 0.2, 1.0, 'tëxt', x'00ff', ''");
        QCOMPARE(results.size(), 1u);
        const Result &r = results[0];
        QVERIFY(!r.isError());
        QCOMPARE(r.rowCount(), 1);
        QCOMPARE(r.type(0, 0), ValueType::Null);
        QCOMPARE(r.type(0, 1), ValueType::Integer);
        QCOMPARE(r.value(0, 1), QByteArrayView("42"));
        QCOMPARE(r.type(0, 2), ValueType::Real);
        // The shortest text that reads back as the same double, not %.15g.
        QCOMPARE(r.value(0, 2), QByteArrayView("0.30000000000000004"));
        QCOMPARE(r.value(0, 3), QByteArrayView("1.0"));
        QCOMPARE(r.type(0, 4), ValueType::Text);
        QCOMPARE(QString::fromUtf8(r.value(0, 4)), QStringLiteral("tëxt"));
        QCOMPARE(r.type(0, 5), ValueType::Blob);
        QCOMPARE(r.value(0, 5).toByteArray(), QByteArray("\x00\xff", 2));
        QCOMPARE(r.type(0, 6), ValueType::Text);
        QVERIFY(r.value(0, 6).isEmpty());
    }

    void columnsTellWhereTheyComeFrom()
    {
        Connection c;
        openNew(c, QStringLiteral("columns.db"));
        run(c, "CREATE TABLE person(id INTEGER PRIMARY KEY, name TEXT NOT NULL)");
        const auto results = run(c, "SELECT p.name AS who, id + 1 FROM person p");
        QCOMPARE(results.size(), 1u);
        const Result &r = results[0];
        QCOMPARE(r.columnCount(), 2);
        // Issue #2: an alias is the column's name, the table's column its origin.
        QCOMPARE(r.column(0).name, QStringLiteral("who"));
        QCOMPARE(r.column(0).origin, QStringLiteral("name"));
        QCOMPARE(r.column(0).table, QStringLiteral("person"));
        QCOMPARE(r.column(0).database, QStringLiteral("main"));
        QCOMPARE(r.column(0).declaredType, QStringLiteral("TEXT"));
        QVERIFY(r.column(1).table.isEmpty());
    }

    void severalStatementsGiveAResultEach()
    {
        Connection c;
        openNew(c, QStringLiteral("several.db"));
        const auto results = run(c,
                                 "CREATE TABLE t(a); INSERT INTO t VALUES (1), (2), (3);\n"
                                 "-- a comment between\n"
                                 "UPDATE t SET a = a * 10 WHERE a > 1; SELECT * FROM t ORDER BY a");
        QCOMPARE(results.size(), 4u);
        QVERIFY(results[0].changesSchema());
        QVERIFY(!results[0].isReadOnly());
        QCOMPARE(results[1].changes(), 3);
        QCOMPARE(results[1].lastInsertRowid(), 3);
        QVERIFY(!results[1].changesSchema());
        QCOMPARE(results[2].changes(), 2);
        QVERIFY(results[3].isReadOnly());
        QCOMPARE(results[3].rowCount(), 3);
        QCOMPARE(results[3].value(2, 0), QByteArrayView("30"));
    }

    void errorStopsTheQueryAndSaysWhere()
    {
        Connection c;
        openNew(c, QStringLiteral("errors.db"));
        QByteArray sql = "SELECT 1; SELECT * FROM nowhere; SELECT 3";
        auto results = run(c, sql);
        QCOMPARE(results.size(), 2u);
        QVERIFY(!results[0].isError());
        QVERIFY(results[1].isError());
        QVERIFY(results[1].errorMessage().contains(QLatin1String("nowhere")));
        QCOMPARE(results[1].errorCode(), SQLITE_ERROR);
        QCOMPARE(results[1].offset(), sql.indexOf(" SELECT *"));
        // SQLite knows no position for a missing table.
        QCOMPARE(results[1].errorOffset(), -1);

        // For a missing column it does, and it points into the whole SQL sent.
        sql = "SELECT 1; SELECT nocolumn FROM (SELECT 1)";
        results = run(c, sql);
        QCOMPARE(results.size(), 2u);
        QCOMPARE(results[1].errorOffset(), sql.indexOf("nocolumn"));
    }

    void rowsArriveInChunks()
    {
        Connection c;
        openNew(c, QStringLiteral("chunks.db"));
        ExecuteOptions options;
        options.chunkSize = 100;
        const auto results = run(c,
                                 "WITH RECURSIVE n(i) AS (SELECT 1 UNION ALL SELECT i + 1 FROM n "
                                 "WHERE i < 250) SELECT i FROM n",
                                 options);
        QCOMPARE(results.size(), 3u);
        QVERIFY(results[0].isChunk());
        QVERIFY(results[1].isChunk());
        QCOMPARE(results[2].kind(), Result::Kind::Done);
        RowStore rows;
        for (const Result &r : results)
            rows.append(r);
        QCOMPARE(rows.rowCount(), 250);
        QCOMPARE(rows.value(0, 0), QByteArrayView("1"));
        QCOMPARE(rows.value(249, 0), QByteArrayView("250"));
        QCOMPARE(rows.valueAt(100, 0), Value::integer(101));
    }

    void rowLimitTruncatesReadsOnly()
    {
        Connection c;
        openNew(c, QStringLiteral("limit.db"));
        ExecuteOptions options;
        options.maxRows = 10;
        auto results = run(c,
                           "WITH RECURSIVE n(i) AS (SELECT 1 UNION ALL SELECT i + 1 FROM n) "
                           "SELECT i FROM n",
                           options);
        QCOMPARE(results.size(), 1u);
        QCOMPARE(results[0].rowCount(), 10);
        QVERIFY(results[0].isTruncated());

        // A statement that writes runs to its end, whatever it returns.
        run(c, "CREATE TABLE t(a)");
        results
            = run(c, "INSERT INTO t SELECT value FROM generate_series(1, 20) RETURNING a", options);
        if (results.back().isError()) // generate_series is an extension in older SQLites.
            results = run(c,
                          "WITH RECURSIVE n(i) AS (SELECT 1 UNION ALL SELECT i + 1 FROM n WHERE "
                          "i < 20) INSERT INTO t SELECT i FROM n RETURNING a",
                          options);
        QCOMPARE(results.back().changes(), 20);
        QVERIFY(!results.back().isTruncated());
    }

    void bindingsRunTheStatementForEachSet()
    {
        Connection c;
        openNew(c, QStringLiteral("bind.db"));
        run(c, "CREATE TABLE t(i INTEGER, r REAL, s TEXT, b BLOB)");
        ExecuteOptions options;
        options.bindings = {{Value::integer(1), Value::real(1.5), Value::text(QStringLiteral("a")),
                             Value::blob(QByteArray())},
                            {Value::null(), Value::null(), Value::text(QString()),
                             Value::blob(QByteArray("\x01", 1))}};
        auto results = run(c, "INSERT INTO t VALUES (?, ?, ?, ?)", options);
        QCOMPARE(results.size(), 1u);
        QCOMPARE(results[0].changes(), 2);

        results = run(c, "SELECT typeof(i), typeof(r), typeof(s), typeof(b), length(b) FROM t");
        const Result &r = results[0];
        QCOMPARE(r.value(0, 0), QByteArrayView("integer"));
        QCOMPARE(r.value(0, 1), QByteArrayView("real"));
        // An empty blob stays a blob rather than turning into NULL.
        QCOMPARE(r.value(0, 3), QByteArrayView("blob"));
        QCOMPARE(r.value(0, 4), QByteArrayView("0"));
        QCOMPARE(r.value(1, 0), QByteArrayView("null"));
        QCOMPARE(r.value(1, 2), QByteArrayView("text"));
    }

    void failingBindingSaysWhich()
    {
        Connection c;
        openNew(c, QStringLiteral("bindfail.db"));
        run(c, "CREATE TABLE t(i INTEGER PRIMARY KEY)");
        ExecuteOptions options;
        options.bindings = {{Value::integer(1)}, {Value::integer(2)}, {Value::integer(1)}};
        const auto results = run(c, "INSERT INTO t VALUES (?)", options);
        QCOMPARE(results.size(), 1u);
        QVERIFY(results[0].isError());
        QCOMPARE(results[0].failedBinding(), 2);
        QCOMPARE(results[0].errorCode(), SQLITE_CONSTRAINT_PRIMARYKEY);
    }

    void bindingsWantOneStatement()
    {
        Connection c;
        openNew(c, QStringLiteral("bindone.db"));
        ExecuteOptions options;
        options.bindings = {{Value::integer(1)}};
        const auto results = run(c, "SELECT ?; SELECT 2", options);
        QCOMPARE(results.size(), 1u);
        QVERIFY(results[0].isError());
    }

    void transactionIsTracked()
    {
        Connection c;
        openNew(c, QStringLiteral("txn.db"));
        QSignalSpy changed(&c, &Connection::transactionChanged);
        run(c, "BEGIN");
        QVERIFY(c.inTransaction());
        QCOMPARE(changed.size(), 1);
        run(c, "CREATE TABLE t(a)");
        QVERIFY(c.inTransaction());
        run(c, "ROLLBACK");
        QVERIFY(!c.inTransaction());
        QCOMPARE(changed.size(), 2);
        QVERIFY(run(c, "SELECT * FROM t").back().isError());
    }

    void cancelInterruptsALongQuery()
    {
        Connection c;
        openNew(c, QStringLiteral("cancel.db"));
        std::vector<Result> results;
        QObject::connect(&c, &Connection::resultReady, this,
                         [&](const Result &r) { results.push_back(r); });
        QSignalSpy finished(&c, &Connection::queryFinished);
        QVERIFY(c.execute("WITH RECURSIVE n(i) AS (SELECT 1 UNION ALL SELECT i + 1 FROM n) "
                          "SELECT count(*) FROM n; SELECT 'never'"));
        QCOMPARE(c.state(), State::Busy);
        QTest::qWait(100);
        QVERIFY(c.cancel());
        QVERIFY(finished.wait(5000));
        QCOMPARE(results.size(), 1u);
        QVERIFY(results[0].isError());
        QCOMPARE(results[0].errorCode(), SQLITE_INTERRUPT);
        QCOMPARE(c.state(), State::Ready);
        // The connection works on afterwards.
        QCOMPARE(run(c, "SELECT 1").size(), 1u);
    }

    void taskRunsWithTheHandle()
    {
        Connection c;
        openNew(c, QStringLiteral("task.db"));
        QSignalSpy done(&c, &Connection::taskFinished);
        QVERIFY(c.perform([](sqlite3 *db) {
            TaskOutcome outcome;
            outcome.value = QString::fromUtf8(sqlite3_db_filename(db, "main"));
            return outcome;
        }));
        QVERIFY(done.wait());
        const auto outcome = done.first().first().value<TaskOutcome>();
        QVERIFY(outcome.error.isEmpty());
        QVERIFY(outcome.value.toString().endsWith(QLatin1String("task.db")));
    }

    void foreignKeysAreOn()
    {
        Connection c;
        openNew(c, QStringLiteral("fk.db"));
        const auto results = run(c, "PRAGMA foreign_keys");
        QCOMPARE(results[0].value(0, 0), QByteArrayView("1"));
    }

    void readOnlyRefusesWrites()
    {
        {
            Connection c;
            openNew(c, QStringLiteral("ro.db"));
            run(c, "CREATE TABLE t(a)");
        }
        Connection c;
        OpenOptions options;
        options.readOnly = true;
        c.open(path(QStringLiteral("ro.db")), options);
        QTRY_COMPARE(c.state(), State::Ready);
        QVERIFY(c.isReadOnly());
        const auto results = run(c, "INSERT INTO t VALUES (1)");
        QVERIFY(results.back().isError());
        QCOMPARE(results.back().errorCode() & 0xff, SQLITE_READONLY);
    }

    void closeDuringAQueryDropsItsResults()
    {
        Connection c;
        openNew(c, QStringLiteral("close.db"));
        int results = 0;
        QObject::connect(&c, &Connection::resultReady, this, [&](const Result &) { ++results; });
        QVERIFY(c.execute("WITH RECURSIVE n(i) AS (SELECT 1 UNION ALL SELECT i + 1 FROM n) "
                          "SELECT count(*) FROM n"));
        c.close();
        QCOMPARE(c.state(), State::Closed);
        QTest::qWait(200);
        QCOMPARE(results, 0);
        // And it opens again.
        c.open(path(QStringLiteral("close.db")));
        QTRY_COMPARE(c.state(), State::Ready);
    }

    void deletedWhileBusy()
    {
        auto *c = new Connection;
        openNew(*c, QStringLiteral("deleted.db"));
        QVERIFY(c->execute("WITH RECURSIVE n(i) AS (SELECT 1 UNION ALL SELECT i + 1 FROM n) "
                           "SELECT count(*) FROM n"));
        delete c; // Must neither block nor crash when the worker replies.
        QTest::qWait(200);
    }
};

QTEST_GUILESS_MAIN(TestConnection)
#include "tst_connection.moc"
