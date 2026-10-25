// SPDX-FileCopyrightText: 2026 Petr Vanek
// SPDX-License-Identifier: GPL-3.0-or-later

#include "TestDatabase.h"

#include "catalog/Dump.h"

#include <QFile>
#include <QTest>
#include <QTextStream>

using namespace sqliteman::catalog;

class TestDump : public QObject
{
    Q_OBJECT

private:
    QString dump(TestDatabase &db, const DumpOptions &options)
    {
        const auto outcome = db.perform([options](sqlite3 *handle) {
            QString text;
            QTextStream out(&text);
            sqliteman::sqlite::TaskOutcome o;
            o.error = dumpDatabase(handle, options, out);
            o.value = text;
            return o;
        });
        if (!outcome.ok())
            qWarning("%s", qPrintable(outcome.error));
        return outcome.task.value.toString();
    }

    void fill(TestDatabase &db)
    {
        db.exec("CREATE TABLE person (id INTEGER PRIMARY KEY AUTOINCREMENT, name TEXT, photo BLOB, "
                "score REAL);"
                "CREATE TABLE \"odd name\" (\"x y\" TEXT);"
                "CREATE INDEX person_name ON person (name);"
                "CREATE VIEW names AS SELECT name FROM person;"
                "CREATE TRIGGER person_upper AFTER INSERT ON person BEGIN "
                "UPDATE person SET name = upper(name) WHERE id = NEW.id; END;"
                "CREATE VIRTUAL TABLE docs USING fts5(body);"
                "INSERT INTO person (name, photo, score) VALUES ('ann', x'00ff', 0.1), "
                "('bob''s', NULL, 1e300);"
                "INSERT INTO \"odd name\" VALUES ('line\nbreak');"
                "INSERT INTO docs VALUES ('full text');");
    }

private Q_SLOTS:
    void readsBackTheSame()
    {
        TestDatabase original;
        fill(original);
        const QString sql = dump(original, {});
        QVERIFY(sql.startsWith(QLatin1String("PRAGMA foreign_keys=OFF;\nBEGIN TRANSACTION;\n")));
        QVERIFY(sql.endsWith(QLatin1String("COMMIT;\n")));
        QVERIFY(sql.contains(QLatin1String("INSERT INTO \"person\" VALUES(1,'ANN',X'00FF',0.1);")));
        // The full-text index is its shadow tables; the dump fills the table itself.
        QVERIFY(!sql.contains(QLatin1String("docs_data")));

        TestDatabase copy(QStringLiteral("copy.db"));
        copy.exec(sql.toUtf8());
        for (const QByteArray &check :
             {QByteArray("SELECT group_concat(name || ':' || quote(photo) || ':' || score, '|') "
                         "FROM person"),
              QByteArray("SELECT \"x y\" FROM \"odd name\""),
              QByteArray("SELECT group_concat(name) FROM names"),
              QByteArray("SELECT body FROM docs WHERE docs MATCH 'text'"),
              QByteArray("SELECT seq FROM sqlite_sequence WHERE name = 'person'"),
              QByteArray("SELECT count(*) FROM sqlite_schema")}) {
            QCOMPARE(copy.value(check), original.value(check));
        }
        // The trigger came along, and works.
        copy.exec("INSERT INTO person (name) VALUES ('cid')");
        QCOMPARE(copy.value("SELECT name FROM person WHERE id = 3"), QStringLiteral("CID"));
    }

    void schemaOnly()
    {
        TestDatabase db;
        fill(db);
        DumpOptions options;
        options.data = false;
        const QString sql = dump(db, options);
        QVERIFY(sql.contains(QLatin1String("CREATE TABLE person")));
        QVERIFY(sql.contains(QLatin1String("CREATE INDEX person_name")));
        QVERIFY(!sql.contains(QLatin1String("INSERT INTO")));
        QVERIFY(!sql.contains(QLatin1String("sqlite_sequence")));
    }

    void anAttachedDatabase()
    {
        TestDatabase db;
        db.exec(QStringLiteral("ATTACH '%1/other.db' AS other").arg(db.dir()).toUtf8());
        db.exec(
            "CREATE TABLE other.t (a); INSERT INTO other.t VALUES (1); CREATE TABLE main.m (b)");
        DumpOptions options;
        options.schema = QStringLiteral("other");
        const QString sql = dump(db, options);
        QVERIFY(sql.contains(QLatin1String("CREATE TABLE t (a);")));
        QVERIFY(sql.contains(QLatin1String("INSERT INTO \"t\" VALUES(1);")));
        QVERIFY(!sql.contains(QLatin1String("CREATE TABLE m")));
    }

    void toAFile()
    {
        TestDatabase db;
        fill(db);
        const QString path = db.dir() + QStringLiteral("/dump.sql");
        const auto outcome = db.perform(dumpTask({}, path));
        QVERIFY2(outcome.ok(), qPrintable(outcome.error));
        QFile file(path);
        QVERIFY(file.open(QIODevice::ReadOnly));
        QVERIFY(file.readAll().contains("CREATE VIEW names"));
        QVERIFY(!db.connection().inTransaction());
    }
};

QTEST_GUILESS_MAIN(TestDump)
#include "tst_dump.moc"
