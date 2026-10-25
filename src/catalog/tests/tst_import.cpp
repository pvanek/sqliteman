// SPDX-FileCopyrightText: 2026 Petr Vanek
// SPDX-License-Identifier: GPL-3.0-or-later

#include "TestDatabase.h"

#include "catalog/Import.h"

#include <QFile>
#include <QSignalSpy>
#include <QTest>

using namespace sqliteman::catalog;

class TestImport : public QObject
{
    Q_OBJECT

private:
    QString write(TestDatabase &db, const QByteArray &content)
    {
        const QString path = db.dir() + QStringLiteral("/data.csv");
        QFile file(path);
        if (!file.open(QIODevice::WriteOnly))
            return {};
        file.write(content);
        return path;
    }

    ImportPlan peoplePlan(const QString &path)
    {
        ImportPlan plan;
        plan.path = path;
        plan.schema = QStringLiteral("main");
        plan.table = QStringLiteral("people");
        plan.columns = {{0, QStringLiteral("id"), QStringLiteral("INTEGER")},
                        {1, QStringLiteral("name"), QStringLiteral("TEXT")}};
        plan.batchRows = 2; // Several batches, even for a small file.
        return plan;
    }

    // Runs the job to its end; returns (ok, message).
    std::pair<bool, QString> runJob(ImportJob &job)
    {
        QSignalSpy finished(&job, &ImportJob::finished);
        job.start();
        if (finished.isEmpty() && !finished.wait(10'000))
            return {false, QStringLiteral("timed out")};
        return {finished.first().at(0).toBool(), finished.first().at(1).toString()};
    }

private Q_SLOTS:
    void importsIntoATable()
    {
        TestDatabase db;
        db.exec("CREATE TABLE people (id INTEGER PRIMARY KEY, name TEXT, added TEXT DEFAULT 'x')");
        const QString path = write(db, "id,name\n1,Alice\n2,\"Bo, the builder\"\n3,\n007,Bond\n");
        ImportJob job(&db.runner(), peoplePlan(path));
        QSignalSpy progress(&job, &ImportJob::progress);
        const auto [ok, message] = runJob(job);
        QVERIFY2(ok, qPrintable(message));
        QCOMPARE(job.rows(), 4);
        QVERIFY(progress.size() >= 2);
        QCOMPARE(db.value("SELECT count(*) FROM people"), QStringLiteral("4"));
        QCOMPARE(db.value("SELECT name FROM people WHERE id = 2"),
                 QStringLiteral("Bo, the builder"));
        QCOMPARE(db.value("SELECT typeof(name) FROM people WHERE id = 3"), QStringLiteral("null"));
        QCOMPARE(db.value("SELECT added FROM people WHERE id = 1"), QStringLiteral("x"));
        QCOMPARE(db.value("SELECT name FROM people WHERE id = 7"), QStringLiteral("Bond"));
        QVERIFY(!db.connection().inTransaction());
    }

    void failsAsAWhole()
    {
        TestDatabase db;
        db.exec("CREATE TABLE people (id INTEGER PRIMARY KEY, name TEXT)");
        // The third data row breaks the primary key, in the second batch.
        const QString path = write(db, "id,name\n1,a\n2,b\n1,c\n4,d\n");
        ImportJob job(&db.runner(), peoplePlan(path));
        const auto [ok, message] = runJob(job);
        QVERIFY(!ok);
        QVERIFY2(message.contains(QLatin1String("Row 3")), qPrintable(message));
        QCOMPARE(db.value("SELECT count(*) FROM people"), QStringLiteral("0"));
        QVERIFY(!db.connection().inTransaction());
    }

    void insideATransactionOnlyTheImportIsUndone()
    {
        TestDatabase db;
        db.exec("CREATE TABLE people (id INTEGER PRIMARY KEY, name TEXT); BEGIN;"
                "INSERT INTO people VALUES (100, 'mine')");
        const QString path = write(db, "id,name\n1,a\n1,b\n");
        ImportJob job(&db.runner(), peoplePlan(path));
        QVERIFY(!runJob(job).first);
        QVERIFY(db.connection().inTransaction());
        QCOMPARE(db.value("SELECT group_concat(id) FROM people"), QStringLiteral("100"));
        db.exec("ROLLBACK");
    }

    void createsTheTable()
    {
        TestDatabase db;
        const QString path = write(db, "n;when\n1;2026-10-24\n2;2026-10-25\n");
        ImportPlan plan;
        plan.path = path;
        plan.csv.delimiter = QLatin1Char(';');
        plan.schema = QStringLiteral("main");
        plan.table = QStringLiteral("New Table");
        plan.create = true;
        plan.columns = {{0, QStringLiteral("n"), QStringLiteral("INTEGER")},
                        {1, QStringLiteral("when"), QStringLiteral("TEXT")}};
        ImportJob job(&db.runner(), plan);
        const auto [ok, message] = runJob(job);
        QVERIFY2(ok, qPrintable(message));
        QCOMPARE(db.value("SELECT sum(n) FROM \"New Table\""), QStringLiteral("3"));
        QCOMPARE(db.value("SELECT typeof(n) FROM \"New Table\" LIMIT 1"),
                 QStringLiteral("integer"));
    }

    void emptiesTheTableFirst()
    {
        TestDatabase db;
        db.exec("CREATE TABLE people (id INTEGER PRIMARY KEY, name TEXT);"
                "INSERT INTO people VALUES (1, 'old')");
        ImportPlan plan = peoplePlan(write(db, "id,name\n1,new\n"));
        plan.truncate = true;
        ImportJob job(&db.runner(), plan);
        QVERIFY(runJob(job).first);
        QCOMPARE(db.value("SELECT name FROM people"), QStringLiteral("new"));
    }

    void reportsAMissingFile()
    {
        TestDatabase db;
        ImportJob job(&db.runner(), peoplePlan(db.dir() + QStringLiteral("/missing.csv")));
        const auto [ok, message] = runJob(job);
        QVERIFY(!ok);
        QVERIFY(message.contains(QLatin1String("missing.csv")));
    }

    void cancels()
    {
        TestDatabase db;
        db.exec("CREATE TABLE people (id INTEGER PRIMARY KEY, name TEXT)");
        QByteArray content = "id,name\n";
        for (int i = 1; i <= 20000; ++i)
            content += QByteArray::number(i) + ",x\n";
        ImportPlan plan = peoplePlan(write(db, content));
        plan.batchRows = 100;
        ImportJob job(&db.runner(), plan);
        QSignalSpy finished(&job, &ImportJob::finished);
        connect(&job, &ImportJob::progress, &job, [&job] { job.cancel(); });
        job.start();
        QVERIFY(finished.wait(10'000));
        QVERIFY(!finished.first().at(0).toBool());
        QCOMPARE(db.value("SELECT count(*) FROM people"), QStringLiteral("0"));
    }
};

QTEST_GUILESS_MAIN(TestImport)
#include "tst_import.moc"
