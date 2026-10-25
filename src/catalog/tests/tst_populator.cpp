// SPDX-FileCopyrightText: 2026 Petr Vanek
// SPDX-License-Identifier: GPL-3.0-or-later

#include "TestDatabase.h"

#include "catalog/Populator.h"

#include <QRandomGenerator>
#include <QTest>

using namespace sqliteman::catalog;
namespace populator = sqliteman::catalog::populator;
using populator::Action;

class TestPopulator : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void choosesActionsByType()
    {
        TestDatabase db;
        db.exec("CREATE TABLE t (id INTEGER PRIMARY KEY, name VARCHAR(5), price REAL, born DATE, "
                "twice INT AS (id * 2))");
        const auto columns
            = populator::columnsOf(*db.snapshot()->find(QString(), QStringLiteral("t")));
        QCOMPARE(columns.size(), 5u);
        QCOMPARE(columns[0].action, Action::AutoNumber);
        QCOMPARE(columns[1].action, Action::RandomText);
        QCOMPARE(columns[1].size, 5); // From VARCHAR(5).
        QCOMPARE(columns[2].action, Action::RandomNumber);
        QCOMPARE(columns[3].action, Action::DateRandom);
        QCOMPARE(columns[4].action, Action::Ignore); // Generated.
    }

    void fillsATable()
    {
        TestDatabase db;
        db.exec("CREATE TABLE t (id INTEGER PRIMARY KEY, code TEXT, n INT, at TEXT, unix INT, "
                "jd REAL, fixed TEXT, seq INT, skipped TEXT DEFAULT 'd'); INSERT INTO t (id) "
                "VALUES (10)");
        auto columns = populator::columnsOf(*db.snapshot()->find(QString(), QStringLiteral("t")));
        columns[1].action = Action::PrefixedText;
        columns[1].value = QStringLiteral("code-");
        columns[2].size = 3;
        columns[3].action = Action::DateNow;
        columns[4].action = Action::DateRandomUnix;
        columns[5].action = Action::DateNowJulian;
        columns[6].action = Action::Static;
        columns[6].value = QStringLiteral("same");
        columns[7].action = Action::AutoNumberFrom;
        columns[7].value = QStringLiteral("100");
        columns[8].action = Action::Ignore;

        const QByteArray maxQuery
            = populator::maximumQuery(QStringLiteral("main"), QStringLiteral("t"), columns);
        QCOMPARE(maxQuery, QByteArray("SELECT coalesce(max(id), 0) AS id FROM main.t"));
        QHash<QString, qint64> maxima {{QStringLiteral("id"), db.value(maxQuery).toLongLong()}};

        QRandomGenerator random(42);
        const QDateTime now = QDateTime::fromSecsSinceEpoch(1'761'300'000, QTimeZone::UTC);
        sqliteman::sqlite::ExecuteOptions options;
        options.bindings = populator::rows(columns, 50, maxima, random, now);
        const QByteArray insert = populator::insertStatement(QStringLiteral("main"),
                                                             QStringLiteral("t"), columns, false);
        const auto outcome = db.run(insert, options);
        QVERIFY2(outcome.ok(), qPrintable(outcome.error));
        QCOMPARE(outcome.results.back().changes(), 50);

        QCOMPARE(db.value("SELECT min(id) || '-' || max(id) FROM t WHERE id > 10"),
                 QStringLiteral("11-60"));
        QCOMPARE(db.value("SELECT code FROM t WHERE id = 11"), QStringLiteral("code-1"));
        QCOMPARE(db.value("SELECT max(n) < 1000 FROM t"), QStringLiteral("1"));
        // Written so SQLite's own date functions read it back.
        QCOMPARE(db.value("SELECT count(*) FROM t WHERE datetime(at) = at"), QStringLiteral("50"));
        QCOMPARE(db.value("SELECT unixepoch(at) FROM t WHERE id = 11"),
                 QStringLiteral("1761300000"));
        QCOMPARE(db.value("SELECT round(julianday(at) - jd, 6) FROM t WHERE id = 11"),
                 QStringLiteral("0.0"));
        QCOMPARE(db.value("SELECT count(DISTINCT fixed) FROM t WHERE id > 10"),
                 QStringLiteral("1"));
        QCOMPARE(db.value("SELECT max(seq) FROM t"), QStringLiteral("150"));
        QCOMPARE(db.value("SELECT count(*) FROM t WHERE skipped = 'd'"), QStringLiteral("51"));
    }

    void ignoringConstraintErrors()
    {
        TestDatabase db;
        db.exec("CREATE TABLE t (id INTEGER PRIMARY KEY, v TEXT UNIQUE)");
        std::vector<populator::Column> columns(1);
        columns[0].name = QStringLiteral("v");
        columns[0].action = Action::Static;
        columns[0].value = QStringLiteral("dup");
        QRandomGenerator random(1);
        sqliteman::sqlite::ExecuteOptions options;
        options.bindings = populator::rows(columns, 5, {}, random, QDateTime::currentDateTimeUtc());
        const auto outcome = db.run(
            populator::insertStatement(QString(), QStringLiteral("t"), columns, true), options);
        QVERIFY(outcome.ok());
        QCOMPARE(outcome.results.back().changes(), 1);
        QVERIFY(!db.run(populator::insertStatement(QString(), QStringLiteral("t"), columns, false),
                        options)
                     .ok());
    }

    void populatesAllOrNothing()
    {
        TestDatabase db;
        db.exec("CREATE TABLE t (id INTEGER PRIMARY KEY, name TEXT, code INT UNIQUE);"
                "INSERT INTO t VALUES (5, 'x', 1)");
        auto columns = populator::columnsOf(*db.snapshot()->find(QString(), QStringLiteral("t")));
        columns[2].action = Action::Static; // Every row the same: breaks UNIQUE.
        columns[2].value = QStringLiteral("2");
        auto outcome = db.perform([columns](sqlite3 *h) {
            return populator::populate(h, QStringLiteral("main"), QStringLiteral("t"), columns, 10,
                                       false);
        });
        QVERIFY(!outcome.ok());
        QCOMPARE(db.value("SELECT count(*) FROM t"), QStringLiteral("1"));

        // Skipping what breaks it: the first row goes in, the rest not.
        outcome = db.perform([columns](sqlite3 *h) {
            return populator::populate(h, QStringLiteral("main"), QStringLiteral("t"), columns, 10,
                                       true);
        });
        QVERIFY2(outcome.ok(), qPrintable(outcome.error));
        QCOMPARE(outcome.task.value.toInt(), 1);
        QCOMPARE(db.value("SELECT max(id) FROM t"), QStringLiteral("6"));
        QVERIFY(!db.connection().inTransaction());
    }
};

QTEST_GUILESS_MAIN(TestPopulator)
#include "tst_populator.moc"
