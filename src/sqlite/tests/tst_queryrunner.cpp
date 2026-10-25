// SPDX-FileCopyrightText: 2026 Petr Vanek
// SPDX-License-Identifier: GPL-3.0-or-later

#include "sqlite/QueryRunner.h"

#include <QTemporaryDir>
#include <QTest>

#include <sqlite3.h>

#include <memory>
#include <optional>

using sqliteman::sqlite::Connection;
using sqliteman::sqlite::ExecuteOptions;
using sqliteman::sqlite::OpenOptions;
using sqliteman::sqlite::QueryOutcome;
using sqliteman::sqlite::QueryRunner;
using sqliteman::sqlite::Result;
using sqliteman::sqlite::TaskOutcome;
using State = Connection::State;

class TestQueryRunner : public QObject
{
    Q_OBJECT

private:
    QTemporaryDir m_dir;

    void open(Connection &c, const QString &name)
    {
        OpenOptions options;
        options.create = true;
        c.open(m_dir.filePath(name), options);
    }

private Q_SLOTS:
    void queuedInOrderBeforeOpening()
    {
        Connection c;
        QueryRunner runner(&c);
        QStringList order;
        for (int i = 1; i <= 3; ++i) {
            runner.run("SELECT " + QByteArray::number(i), this, [&, i](const QueryOutcome &o) {
                QVERIFY(o.ok());
                QCOMPARE(o.results.size(), 1u);
                order << QString::fromUtf8(o.results[0].value(0, 0));
                QCOMPARE(order.last(), QString::number(i));
            });
        }
        QCOMPARE(runner.pending(), 3);
        open(c, QStringLiteral("order.db")); // Queries wait for the database.
        QTRY_COMPARE(order.size(), 3);
        QCOMPARE(runner.pending(), 0);
    }

    void errorDoesNotStopTheQueue()
    {
        Connection c;
        open(c, QStringLiteral("errors.db"));
        QueryRunner runner(&c);
        std::optional<QueryOutcome> failed, next;
        runner.run("SELECT * FROM missing_table", this, [&](const QueryOutcome &o) { failed = o; });
        runner.run("SELECT 2", this, [&](const QueryOutcome &o) { next = o; });
        QTRY_VERIFY(next.has_value());
        QVERIFY(!failed->ok());
        QCOMPARE(failed->errorCode, SQLITE_ERROR);
        QVERIFY(failed->error.contains(QLatin1String("missing_table")));
        QVERIFY(next->ok());
    }

    void callbackSkippedWhenContextDeleted()
    {
        Connection c;
        open(c, QStringLiteral("context.db"));
        QueryRunner runner(&c);
        auto context = std::make_unique<QObject>();
        bool called = false;
        bool after = false;
        runner.run("SELECT 1", context.get(), [&](const QueryOutcome &) { called = true; });
        runner.run("SELECT 2", this, [&](const QueryOutcome &) { after = true; });
        context.reset();
        QTRY_VERIFY(after);
        QVERIFY(!called);
    }

    void chunksAreMergedWithoutAStreamer()
    {
        Connection c;
        open(c, QStringLiteral("merge.db"));
        QueryRunner runner(&c);
        ExecuteOptions options;
        options.chunkSize = 7;
        std::optional<QueryOutcome> outcome;
        runner.run(
            "WITH RECURSIVE n(i) AS (SELECT 1 UNION ALL SELECT i + 1 FROM n WHERE i < 50) "
            "SELECT i, 'row ' || i FROM n",
            this, [&](const QueryOutcome &o) { outcome = o; }, options);
        QTRY_VERIFY(outcome.has_value());
        QCOMPARE(outcome->results.size(), 1u);
        const Result rows = outcome->rows();
        QCOMPARE(rows.rowCount(), 50);
        QCOMPARE(rows.value(49, 1), QByteArrayView("row 50"));
    }

    void streamerSeesEveryChunk()
    {
        Connection c;
        open(c, QStringLiteral("stream.db"));
        QueryRunner runner(&c);
        ExecuteOptions options;
        options.chunkSize = 10;
        int chunks = 0;
        int rows = 0;
        std::optional<QueryOutcome> outcome;
        runner.run(
            "WITH RECURSIVE n(i) AS (SELECT 1 UNION ALL SELECT i + 1 FROM n WHERE i < 35) "
            "SELECT i FROM n",
            this, [&](const QueryOutcome &o) { outcome = o; }, options,
            [&](const Result &r) {
                chunks += r.isChunk() ? 1 : 0;
                rows += r.rowCount();
            });
        QTRY_VERIFY(outcome.has_value());
        QCOMPARE(chunks, 3);
        QCOMPARE(rows, 35);
        QCOMPARE(outcome->results.size(), 1u); // The final result only.
    }

    void cancelQueuedQuery()
    {
        Connection c;
        open(c, QStringLiteral("cancelqueued.db"));
        QueryRunner runner(&c);
        std::optional<QueryOutcome> first, second;
        runner.run("SELECT 1", this, [&](const QueryOutcome &o) { first = o; });
        const quint64 id = runner.run("SELECT 2", this, [&](const QueryOutcome &o) { second = o; });
        runner.cancel(id);
        QVERIFY(second.has_value());
        QCOMPARE(second->errorCode, SQLITE_INTERRUPT);
        QTRY_VERIFY(first.has_value());
        QVERIFY(first->ok());
    }

    void cancelRunningQuery()
    {
        Connection c;
        open(c, QStringLiteral("cancelrunning.db"));
        QTRY_COMPARE(c.state(), State::Ready);
        QueryRunner runner(&c);
        std::optional<QueryOutcome> outcome;
        const quint64 id
            = runner.run("WITH RECURSIVE n(i) AS (SELECT 1 UNION ALL SELECT i + 1 FROM n) "
                         "SELECT count(*) FROM n",
                         this, [&](const QueryOutcome &o) { outcome = o; });
        QCOMPARE(runner.current(), id);
        QTest::qWait(50);
        runner.cancel(id);
        QTRY_VERIFY(outcome.has_value());
        QCOMPARE(outcome->errorCode, SQLITE_INTERRUPT);
    }

    void closingFailsWhatIsPending()
    {
        Connection c;
        open(c, QStringLiteral("closing.db"));
        QueryRunner runner(&c);
        int failed = 0;
        for (int i = 0; i < 3; ++i)
            runner.run("SELECT 1", this, [&](const QueryOutcome &o) { failed += o.ok() ? 0 : 1; });
        c.close();
        QCOMPARE(failed, 3);
        QCOMPARE(runner.pending(), 0);
    }

    void queryOnAFailedDatabaseFailsLater()
    {
        Connection c;
        c.open(m_dir.filePath(QStringLiteral("does/not/exist.db")));
        QTRY_COMPARE(c.state(), State::Failed);
        QueryRunner runner(&c);
        std::optional<QueryOutcome> outcome;
        runner.run("SELECT 1", this, [&](const QueryOutcome &o) { outcome = o; });
        QVERIFY(!outcome.has_value()); // Not from within run().
        QTRY_VERIFY(outcome.has_value());
        QVERIFY(!outcome->ok());
    }

    void taskBetweenQueries()
    {
        Connection c;
        open(c, QStringLiteral("task.db"));
        QueryRunner runner(&c);
        std::optional<QueryOutcome> query, task, failing;
        runner.run("CREATE TABLE t(a)", this, [&](const QueryOutcome &o) { query = o; });
        runner.perform(
            [](sqlite3 *db) {
                TaskOutcome outcome;
                outcome.value = sqlite3_get_autocommit(db);
                return outcome;
            },
            this, [&](const QueryOutcome &o) { task = o; });
        runner.perform([](sqlite3 *) { return TaskOutcome {QStringLiteral("no luck"), {}}; }, this,
                       [&](const QueryOutcome &o) { failing = o; });
        QTRY_VERIFY(failing.has_value());
        QVERIFY(query->ok());
        QVERIFY(task->ok());
        QCOMPARE(task->task.value.toInt(), 1);
        QCOMPARE(failing->error, QStringLiteral("no luck"));
    }
};

QTEST_GUILESS_MAIN(TestQueryRunner)
#include "tst_queryrunner.moc"
