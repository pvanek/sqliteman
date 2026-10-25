// SPDX-FileCopyrightText: 2026 Petr Vanek
// SPDX-License-Identifier: GPL-3.0-or-later

#include "TestDatabase.h"

#include "catalog/AlterTable.h"

#include <QTest>

using namespace sqliteman::catalog;

namespace {

TableDef parsed(TestDatabase &db, const QString &table,
                const QString &schema = QStringLiteral("main"))
{
    const QString sql
        = db.value(QStringLiteral("SELECT sql FROM \"%1\".sqlite_schema WHERE name = '%2'")
                       .arg(schema, table)
                       .toUtf8());
    TableDef t = parseCreateTable(sql).value_or(TableDef {});
    t.schema = schema;
    return t;
}

// The table as it is, each column naming itself as its original.
AlterRequest unchanged(const TableDef &before)
{
    AlterRequest r;
    r.before = before;
    r.after = before;
    for (ColumnDef &c : r.after.columns)
        c.originalName = c.name;
    return r;
}

sqliteman::sqlite::QueryOutcome alter(TestDatabase &db, const AlterRequest &request)
{
    return db.perform([request](sqlite3 *handle) { return alterTable(handle, request); });
}

} // namespace

class TestAlterTable : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void renamesKeepTheirCase()
    {
        // Issue #10: sqliteman 1.2 lowercased table names, and an alter lost
        // their case for good.
        TestDatabase db;
        db.exec("CREATE TABLE MyTable (Id INTEGER PRIMARY KEY, Name TEXT)");
        AlterRequest r = unchanged(parsed(db, QStringLiteral("MyTable")));
        r.after.name = QStringLiteral("MyRenamedTable");
        r.after.columns[1].name = QStringLiteral("FullName");
        const AlterPlan plan = planAlterTable(r, {}, {});
        QVERIFY(!plan.rebuild);
        QCOMPARE(plan.renames.size(), 2);
        QVERIFY(plan.statements.isEmpty());

        const auto outcome = alter(db, r);
        QVERIFY2(outcome.ok(), qPrintable(outcome.error));
        QCOMPARE(db.value("SELECT name FROM sqlite_schema WHERE type = 'table'"),
                 QStringLiteral("MyRenamedTable"));
        QCOMPARE(db.value("SELECT name FROM pragma_table_info('MyRenamedTable') WHERE cid = 1"),
                 QStringLiteral("FullName"));
    }

    void addAndDropColumnsNatively()
    {
        TestDatabase db;
        db.exec("CREATE TABLE t (a INTEGER, b TEXT, c TEXT); INSERT INTO t VALUES (1, 'x', 'y')");
        AlterRequest r = unchanged(parsed(db, QStringLiteral("t")));
        r.after.columns.erase(r.after.columns.begin() + 1); // Drop b.
        ColumnDef d;
        d.name = QStringLiteral("d");
        d.type = QStringLiteral("INTEGER");
        d.notNull = true;
        d.defaultValue = QStringLiteral("7");
        r.after.columns.push_back(d);
        const AlterPlan plan = planAlterTable(r, {}, {});
        QVERIFY(!plan.rebuild);
        QCOMPARE(plan.statements,
                 (QStringList {QStringLiteral("ALTER TABLE main.t DROP COLUMN b"),
                               QStringLiteral(
                                   "ALTER TABLE main.t ADD COLUMN d INTEGER NOT NULL DEFAULT 7")}));

        QVERIFY(alter(db, r).ok());
        QCOMPARE(db.value("SELECT group_concat(name) FROM pragma_table_info('t')"),
                 QStringLiteral("a,c,d"));
        QCOMPARE(db.value("SELECT a || c || d FROM t"), QStringLiteral("1y7"));
    }

    void whatAlterTableCannotDoIsARebuild_data()
    {
        QTest::addColumn<int>("change");
        QTest::newRow("type") << 0;
        QTest::newRow("not null") << 1;
        QTest::newRow("order") << 2;
        QTest::newRow("primary key column added") << 3;
        QTest::newRow("table constraint") << 4;
        QTest::newRow("drop an indexed column") << 5;
    }

    void whatAlterTableCannotDoIsARebuild()
    {
        QFETCH(int, change);
        TableDef before;
        before.schema = QStringLiteral("main");
        before.name = QStringLiteral("t");
        ColumnDef a;
        a.name = QStringLiteral("a");
        a.type = QStringLiteral("INTEGER");
        ColumnDef b;
        b.name = QStringLiteral("b");
        b.type = QStringLiteral("TEXT");
        before.columns = {a, b};
        AlterRequest r = unchanged(before);
        QStringList indexes;
        switch (change) {
        case 0:
            r.after.columns[1].type = QStringLiteral("BLOB");
            break;
        case 1:
            r.after.columns[1].notNull = true;
            break;
        case 2:
            std::swap(r.after.columns[0], r.after.columns[1]);
            break;
        case 3: {
            ColumnDef id;
            id.name = QStringLiteral("id");
            id.primaryKey = true;
            r.after.columns.push_back(id);
            break;
        }
        case 4:
            r.after.constraints << QStringLiteral("UNIQUE (a, b)");
            break;
        case 5:
            r.after.columns.pop_back();
            indexes << QStringLiteral("CREATE INDEX ix ON t (b)");
            break;
        }
        const AlterPlan plan = planAlterTable(r, indexes, {});
        QVERIFY(plan.rebuild);
        if (change == 5) // The index on b cannot be made again.
            QCOMPARE(plan.warnings.size(), 1);
    }

    void rebuildKeepsRowsIndexesAndTriggers()
    {
        TestDatabase db;
        db.exec(
            "CREATE TABLE t (id INTEGER PRIMARY KEY, name TEXT, gone TEXT);"
            "CREATE INDEX t_name ON t (name);"
            "CREATE TABLE log (msg TEXT);"
            "CREATE TRIGGER t_ins AFTER INSERT ON t BEGIN INSERT INTO log VALUES (NEW.name); END;"
            "CREATE VIEW v AS SELECT name FROM t;"
            "INSERT INTO t (name, gone) VALUES ('a', 'x'), ('b', 'y');");
        AlterRequest r = unchanged(parsed(db, QStringLiteral("t")));
        r.after.columns[1].name = QStringLiteral("title"); // Renamed natively first.
        r.after.columns[1].notNull = true; // Which takes a rebuild.
        r.after.columns.pop_back(); // And gone goes.
        const auto outcome = alter(db, r);
        QVERIFY2(outcome.ok(), qPrintable(outcome.error));

        QCOMPARE(db.value("SELECT group_concat(title, ',') FROM t"), QStringLiteral("a,b"));
        QCOMPARE(db.value("SELECT \"notnull\" FROM pragma_table_info('t') WHERE name = 'title'"),
                 QStringLiteral("1"));
        QCOMPARE(db.value("SELECT count(*) FROM pragma_table_info('t')"), QStringLiteral("2"));
        // The index and the trigger were made again, with the new name in them.
        QCOMPARE(db.value("SELECT name FROM pragma_index_info('t_name')"), QStringLiteral("title"));
        db.exec("DELETE FROM log; INSERT INTO t (title) VALUES ('c')");
        QCOMPARE(db.value("SELECT msg FROM log"), QStringLiteral("c"));
        // The view followed the rename.
        QCOMPARE(db.value("SELECT count(*) FROM v"), QStringLiteral("3"));
        QVERIFY(!db.connection().inTransaction());
        QCOMPARE(db.value("PRAGMA foreign_keys"), QStringLiteral("1"));
        QCOMPARE(db.value("PRAGMA legacy_alter_table"), QStringLiteral("0"));
    }

    void failedStepUndoesEverything()
    {
        TestDatabase db;
        db.exec(
            "CREATE TABLE t (id INTEGER PRIMARY KEY, name TEXT); INSERT INTO t VALUES (1, NULL)");
        AlterRequest r = unchanged(parsed(db, QStringLiteral("t")));
        r.after.name = QStringLiteral("renamed");
        r.after.columns[1].notNull = true; // The NULL row cannot go in.
        const auto outcome = alter(db, r);
        QVERIFY(!outcome.ok());
        QVERIFY(outcome.error.contains(QLatin1String("NOT NULL")));
        // Not even the rename stayed.
        QCOMPARE(db.value("SELECT name FROM sqlite_schema WHERE type = 'table'"),
                 QStringLiteral("t"));
        QCOMPARE(db.value("SELECT count(*) FROM t"), QStringLiteral("1"));
        QVERIFY(!db.connection().inTransaction());
    }

    void foreignKeysAreCheckedAfterARebuild()
    {
        TestDatabase db;
        db.exec("CREATE TABLE parent (id INTEGER PRIMARY KEY);"
                "CREATE TABLE child (id INTEGER PRIMARY KEY, parent INTEGER);"
                "INSERT INTO parent VALUES (1); INSERT INTO child VALUES (1, 2);");
        AlterRequest r = unchanged(parsed(db, QStringLiteral("child")));
        r.after.columns[1].extra = QStringLiteral("REFERENCES parent(id)");
        const auto outcome = alter(db, r);
        QVERIFY(!outcome.ok());
        QVERIFY2(outcome.error.contains(QLatin1String("foreign key")), qPrintable(outcome.error));
        QVERIFY(!db.value("SELECT sql FROM sqlite_schema WHERE name = 'child'")
                     .contains(QLatin1String("REFERENCES")));
    }

    void rebuildInATransactionNeedsForeignKeysOff()
    {
        TestDatabase db;
        db.exec("CREATE TABLE t (a INTEGER, b TEXT)");
        db.exec("BEGIN");
        AlterRequest r = unchanged(parsed(db, QStringLiteral("t")));
        r.after.columns[1].type = QStringLiteral("BLOB");
        auto outcome = alter(db, r);
        QVERIFY(!outcome.ok());
        QVERIFY(db.connection().inTransaction()); // Left as it was.

        // With foreign keys off, it nests in the transaction as a savepoint.
        db.exec("ROLLBACK; PRAGMA foreign_keys = OFF; BEGIN");
        outcome = alter(db, r);
        QVERIFY2(outcome.ok(), qPrintable(outcome.error));
        QVERIFY(db.connection().inTransaction());
        db.exec("COMMIT");
        QCOMPARE(db.value("SELECT type FROM pragma_table_info('t') WHERE name = 'b'"),
                 QStringLiteral("BLOB"));
    }

    void rebuildInAnAttachedDatabase()
    {
        TestDatabase db;
        const QString other = db.dir() + QStringLiteral("/other.db");
        db.exec(QStringLiteral("ATTACH DATABASE '%1' AS other").arg(other).toUtf8());
        db.exec("CREATE TABLE other.t (a INTEGER, b TEXT); CREATE INDEX other.t_b ON t (b);"
                "INSERT INTO other.t VALUES (1, 'x')");
        AlterRequest r = unchanged(parsed(db, QStringLiteral("t"), QStringLiteral("other")));
        r.after.columns[0].type = QStringLiteral("TEXT");
        const auto outcome = alter(db, r);
        QVERIFY2(outcome.ok(), qPrintable(outcome.error));
        // The index went back where its table is, not to main.
        QCOMPARE(db.value("SELECT count(*) FROM other.sqlite_schema WHERE name = 't_b'"),
                 QStringLiteral("1"));
        QCOMPARE(db.value("SELECT count(*) FROM main.sqlite_schema"), QStringLiteral("0"));
        QCOMPARE(db.value("SELECT typeof(a) FROM other.t"), QStringLiteral("text"));
    }

    void generatedColumnsAreComputedAgain()
    {
        TestDatabase db;
        db.exec("CREATE TABLE t (a INTEGER, b INTEGER AS (a * 2), c TEXT);"
                "INSERT INTO t (a, c) VALUES (2, 'x')");
        AlterRequest r = unchanged(parsed(db, QStringLiteral("t")));
        r.after.columns[2].type = QStringLiteral("BLOB");
        const auto outcome = alter(db, r);
        QVERIFY2(outcome.ok(), qPrintable(outcome.error));
        QCOMPARE(db.value("SELECT b FROM t"), QStringLiteral("4"));
    }

    void qualifyingAndRenaming()
    {
        QCOMPARE(qualifyCreate(QStringLiteral("CREATE UNIQUE INDEX IF NOT EXISTS ix ON t(a)"),
                               QStringLiteral("aux")),
                 QStringLiteral("CREATE UNIQUE INDEX IF NOT EXISTS aux.ix ON t(a)"));
        QCOMPARE(qualifyCreate(
                     QStringLiteral("CREATE TRIGGER main.tr AFTER INSERT ON t BEGIN SELECT 1; END"),
                     QStringLiteral("aux")),
                 QStringLiteral("CREATE TRIGGER main.tr AFTER INSERT ON t BEGIN SELECT 1; END"));
        QCOMPARE(renameIdentifier(QStringLiteral("CREATE INDEX ix ON t (\"Old\", 'old')"),
                                  QStringLiteral("old"), QStringLiteral("new name")),
                 QStringLiteral("CREATE INDEX ix ON t (\"new name\", 'old')"));
    }

    void previewShowsTheTransaction()
    {
        TableDef before;
        before.schema = QStringLiteral("main");
        before.name = QStringLiteral("t");
        ColumnDef a;
        a.name = QStringLiteral("a");
        before.columns = {a};
        AlterRequest r = unchanged(before);
        r.after.columns[0].type = QStringLiteral("INTEGER");
        const QString preview = planAlterTable(r, {}, {}).preview();
        QVERIFY(preview.startsWith(QLatin1String("PRAGMA foreign_keys = OFF;\nBEGIN;")));
        QVERIFY(preview.contains(QLatin1String("CREATE TABLE main.sqliteman_new_t")));
        QVERIFY(preview.endsWith(QLatin1String("COMMIT;\nPRAGMA foreign_keys = ON;")));
    }
};

QTEST_GUILESS_MAIN(TestAlterTable)
#include "tst_altertable.moc"
