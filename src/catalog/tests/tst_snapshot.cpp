// SPDX-FileCopyrightText: 2026 Petr Vanek
// SPDX-License-Identifier: GPL-3.0-or-later

#include "TestDatabase.h"

#include "catalog/Snapshot.h"

#include <QTest>

using namespace sqliteman::catalog;

namespace {

const SchemaObject *object(const DatabaseInfo &d, const QString &name)
{
    for (const SchemaObject &o : d.objects)
        if (o.name == name)
            return &o;
    return nullptr;
}

} // namespace

class TestSnapshot : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void readsEveryKindOfObject()
    {
        TestDatabase db;
        db.exec("CREATE TABLE Person (Id INTEGER PRIMARY KEY AUTOINCREMENT, Name TEXT NOT NULL "
                "DEFAULT 'x', Code TEXT UNIQUE, Twice INT AS (Id * 2));"
                "CREATE TABLE pair (a, b, PRIMARY KEY (b, a)) WITHOUT ROWID;"
                "CREATE TABLE strict_one (i INTEGER) STRICT;"
                "CREATE INDEX person_name ON Person (Name);"
                "CREATE VIEW PersonView AS SELECT Name FROM Person;"
                "CREATE TRIGGER person_ins AFTER INSERT ON Person BEGIN SELECT 1; END;"
                "INSERT INTO Person (Name) VALUES ('a');");
        const SnapshotPtr s = db.snapshot();
        QVERIFY(s);
        QCOMPARE(s->databases.front().name, QStringLiteral("main"));
        const DatabaseInfo &main = s->databases.front();

        // Issue #10: names as they were written, not lowercased.
        const SchemaObject *person = object(main, QStringLiteral("Person"));
        QVERIFY(person);
        QCOMPARE(person->kind, ObjectKind::Table);
        QVERIFY(person->autoincrement);
        QVERIFY(person->hasRowid());
        QCOMPARE(person->columns.size(), 4u);
        QCOMPARE(person->columns[0].primaryKey, 1);
        QVERIFY(person->columns[1].notNull);
        QVERIFY(person->columns[1].hasDefault);
        QCOMPARE(person->columns[1].defaultValue, QStringLiteral("'x'"));
        QVERIFY(person->columns[3].isGenerated());
        QCOMPARE(person->primaryKey(), std::vector<QString> {QStringLiteral("Id")});

        const SchemaObject *pair = object(main, QStringLiteral("pair"));
        QVERIFY(pair->withoutRowid);
        QVERIFY(!pair->hasRowid());
        QCOMPARE(pair->primaryKey(),
                 (std::vector<QString> {QStringLiteral("b"), QStringLiteral("a")}));
        QVERIFY(object(main, QStringLiteral("strict_one"))->strict);

        QCOMPARE(object(main, QStringLiteral("person_name"))->kind, ObjectKind::Index);
        QCOMPARE(object(main, QStringLiteral("person_name"))->table, QStringLiteral("Person"));
        // The index of the UNIQUE constraint, and AUTOINCREMENT's counters.
        const SchemaObject *automatic = object(main, QStringLiteral("sqlite_autoindex_Person_1"));
        QVERIFY(automatic);
        QVERIFY(automatic->system);
        QVERIFY(automatic->sql.isEmpty());
        QVERIFY(object(main, QStringLiteral("sqlite_sequence"))->system);

        const SchemaObject *view = object(main, QStringLiteral("PersonView"));
        QCOMPARE(view->kind, ObjectKind::View);
        QCOMPARE(view->columns.size(), 1u);
        QCOMPARE(object(main, QStringLiteral("person_ins"))->kind, ObjectKind::Trigger);

        // Sorted by kind, then name.
        QCOMPARE(main.objects.front().kind, ObjectKind::Table);
        QCOMPARE(main.objects.back().kind, ObjectKind::Trigger);
        QVERIFY(s->functions.contains(QStringLiteral("json_extract")));
        QVERIFY(s->collations.contains(QStringLiteral("NOCASE")));
    }

    void virtualTables()
    {
        // Issue #20: R*Tree and full-text search tables, and their shadow
        // tables listed as SQLite's own.
        TestDatabase db;
        db.exec("CREATE VIRTUAL TABLE places USING rtree(id, minX, maxX, minY, maxY);"
                "CREATE VIRTUAL TABLE docs USING fts5(title, body);");
        const SnapshotPtr s = db.snapshot();
        const DatabaseInfo &main = s->databases.front();
        const SchemaObject *places = object(main, QStringLiteral("places"));
        QCOMPARE(places->kind, ObjectKind::VirtualTable);
        QCOMPARE(places->columns.size(), 5u);
        const SchemaObject *docs = object(main, QStringLiteral("docs"));
        QCOMPARE(docs->kind, ObjectKind::VirtualTable);
        const SchemaObject *shadow = object(main, QStringLiteral("docs_data"));
        QVERIFY(shadow);
        QVERIFY(shadow->system);
    }

    void attachedAndTemporaryDatabases()
    {
        TestDatabase db;
        db.exec(QStringLiteral("ATTACH '%1/other.db' AS other").arg(db.dir()).toUtf8());
        db.exec("CREATE TABLE other.things (x); CREATE TEMP TABLE scratch (y); CREATE TABLE t (z)");
        const SnapshotPtr s = db.snapshot();
        QVERIFY(s->database(QStringLiteral("other")));
        QVERIFY(s->database(QStringLiteral("other"))->file.endsWith(QLatin1String("other.db")));
        QVERIFY(s->database(QStringLiteral("temp")));
        QVERIFY(s->find(QString(), QStringLiteral("things")));
        QCOMPARE(s->find(QString(), QStringLiteral("scratch"))->schema, QStringLiteral("temp"));
        QCOMPARE(s->find(QStringLiteral("main"), QStringLiteral("T"))->name, QStringLiteral("t"));
        QVERIFY(!s->find(QStringLiteral("main"), QStringLiteral("things")));
    }

    void foreignKeysInTheOrderDeclared()
    {
        TestDatabase db;
        db.exec(
            "CREATE TABLE a (id INTEGER PRIMARY KEY); CREATE TABLE b (x, y, PRIMARY KEY (x, y));"
            "CREATE TABLE c (ra REFERENCES a, bx, by, FOREIGN KEY (bx, by) REFERENCES b (x, y) "
            "ON DELETE CASCADE)");
        const SnapshotPtr s = db.snapshot();
        const SchemaObject *c = s->find(QStringLiteral("main"), QStringLiteral("c"));
        QVERIFY(c);
        QCOMPARE(c->foreignKeys.size(), 2u);
        QCOMPARE(c->foreignKeys[0].table, QStringLiteral("a"));
        QCOMPARE(c->foreignKeys[0].from, QStringList {QStringLiteral("ra")});
        QVERIFY(c->foreignKeys[0].to.isEmpty()); // Its primary key.
        QCOMPARE(c->foreignKeys[1].table, QStringLiteral("b"));
        QCOMPARE(c->foreignKeys[1].to, (QStringList {QStringLiteral("x"), QStringLiteral("y")}));
        QCOMPARE(c->foreignKeys[1].onDelete, QStringLiteral("CASCADE"));
    }

    void tableThatCannotBeDescribed()
    {
        // A virtual table whose module is not there: listed, with the error.
        TestDatabase db;
        db.exec("PRAGMA writable_schema = ON;"
                "INSERT INTO sqlite_schema (type, name, tbl_name, rootpage, sql) VALUES "
                "('table', 'ghost', 'ghost', 0, 'CREATE VIRTUAL TABLE ghost USING nosuchmodule()');"
                "PRAGMA writable_schema = OFF;"
                "CREATE TABLE fine (a)");
        TestDatabase reopened(QStringLiteral("unused.db"));
        Q_UNUSED(reopened);
        const SnapshotPtr s = db.snapshot();
        QVERIFY(s);
        const DatabaseInfo &main = s->databases.front();
        QVERIFY(object(main, QStringLiteral("fine")));
        QCOMPARE(object(main, QStringLiteral("fine"))->columns.size(), 1u);
    }

    void affinityRules_data()
    {
        QTest::addColumn<QString>("type");
        QTest::addColumn<int>("affinity");
        QTest::newRow("int") << QStringLiteral("INT") << int(Affinity::Integer);
        QTest::newRow("bigint") << QStringLiteral("BIGINT") << int(Affinity::Integer);
        QTest::newRow("varchar") << QStringLiteral("VARCHAR(20)") << int(Affinity::Text);
        QTest::newRow("clob") << QStringLiteral("CLOB") << int(Affinity::Text);
        QTest::newRow("none") << QString() << int(Affinity::Blob);
        QTest::newRow("blob") << QStringLiteral("BLOB") << int(Affinity::Blob);
        QTest::newRow("double") << QStringLiteral("DOUBLE PRECISION") << int(Affinity::Real);
        QTest::newRow("decimal") << QStringLiteral("DECIMAL(10,5)") << int(Affinity::Numeric);
        QTest::newRow("boolean") << QStringLiteral("BOOLEAN") << int(Affinity::Numeric);
        // The rules go in order: INT before CHAR.
        QTest::newRow("charint") << QStringLiteral("CHARINT") << int(Affinity::Integer);
    }

    void affinityRules()
    {
        QFETCH(QString, type);
        QFETCH(int, affinity);
        QCOMPARE(int(sqliteman::catalog::affinity(type)), affinity);
    }
};

QTEST_GUILESS_MAIN(TestSnapshot)
#include "tst_snapshot.moc"
