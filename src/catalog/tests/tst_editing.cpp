// SPDX-FileCopyrightText: 2026 Petr Vanek
// SPDX-License-Identifier: GPL-3.0-or-later

#include "TestDatabase.h"

#include "catalog/Editing.h"

#include <QTest>

using namespace sqliteman::catalog;
using sqliteman::sqlite::Value;

namespace {

// Runs the statements as the editor saves them; each must change one row.
bool save(TestDatabase &db, const std::vector<DmlStatement> &statements)
{
    for (const DmlStatement &s : statements) {
        sqliteman::sqlite::ExecuteOptions options;
        options.bindings = {s.parameters};
        const auto outcome = db.run(s.sql, options);
        if (!outcome.ok() || outcome.results.back().changes() != 1)
            return false;
    }
    return true;
}

} // namespace

class TestEditing : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void rowidTableByItsIntegerKey()
    {
        TestDatabase db;
        db.exec("CREATE TABLE t (id INTEGER PRIMARY KEY, name TEXT, n INT); "
                "INSERT INTO t VALUES (1, 'a', 10), (2, 'b', 20)");
        const auto rows = db.rows("SELECT name, id AS key, n * 2 FROM t ORDER BY id");
        const EditTarget target = editTarget(rows, *db.snapshot());
        QVERIFY2(target.editable(), qPrintable(target.reason));
        QVERIFY(target.keyIsRowid);
        QCOMPARE(target.key, std::vector<int> {1});
        QCOMPARE(target.columns[0], QStringLiteral("name"));
        QVERIFY(target.writable(0));
        QVERIFY(!target.writable(2)); // Computed.
        QCOMPARE(target.affinity[0], Affinity::Text);

        RowEdit update;
        update.key = {rows.valueAt(1, 1)};
        update.values = {{0, Value::text(QStringLiteral("B's"))}};
        RowEdit remove;
        remove.kind = RowEdit::Kind::Delete;
        remove.key = {rows.valueAt(0, 1)};
        RowEdit insert;
        insert.kind = RowEdit::Kind::Insert;
        insert.values = {{0, Value::text(QStringLiteral("c"))}};
        const auto statements = dmlStatements(target, {update, remove, insert});
        QCOMPARE(statements.size(), 3u);
        QCOMPARE(statements[0].sql, QByteArray("UPDATE main.t SET name = ?1 WHERE rowid IS ?2"));
        QCOMPARE(statements[0].preview(),
                 QStringLiteral("UPDATE main.t SET name = 'B''s' WHERE rowid IS 2"));
        QVERIFY(save(db, statements));
        QCOMPARE(db.value("SELECT group_concat(name, ',') FROM t"), QStringLiteral("B's,c"));
    }

    void tableWithoutKeyNeedsItsRowid()
    {
        TestDatabase db;
        db.exec("CREATE TABLE t (name TEXT); INSERT INTO t VALUES ('a'), ('a')");
        EditTarget target = editTarget(db.rows("SELECT name FROM t"), *db.snapshot());
        QVERIFY(!target.editable());
        QVERIFY(target.reason.contains(QLatin1String("rowid")));

        // The table page asks for it.
        const auto rows = db.rows("SELECT rowid, * FROM t");
        target = editTarget(rows, *db.snapshot());
        QVERIFY2(target.editable(), qPrintable(target.reason));
        QVERIFY(target.keyIsRowid);
        QVERIFY(!target.writable(0)); // The rowid itself is not edited.
        RowEdit update;
        update.key = {rows.valueAt(1, 0)};
        update.values = {{1, Value::text(QStringLiteral("b"))}};
        QVERIFY(save(db, dmlStatements(target, {update})));
        QCOMPARE(db.value("SELECT group_concat(name, ',') FROM t ORDER BY rowid"),
                 QStringLiteral("a,b"));
    }

    void compositeKeyWithoutRowid()
    {
        TestDatabase db;
        db.exec("CREATE TABLE t (a TEXT, b INT, v TEXT, PRIMARY KEY (b, a)) WITHOUT ROWID;"
                "INSERT INTO t VALUES ('x', 1, 'one'), ('x', 2, 'two')");
        auto rows = db.rows("SELECT v, a, b FROM t ORDER BY b");
        const EditTarget target = editTarget(rows, *db.snapshot());
        QVERIFY2(target.editable(), qPrintable(target.reason));
        QVERIFY(!target.keyIsRowid);
        QCOMPARE(target.key, (std::vector<int> {2, 1})); // In key order: b, then a.
        RowEdit update;
        update.key = {rows.valueAt(1, 2), rows.valueAt(1, 1)};
        update.values = {{0, Value::text(QStringLiteral("deux"))}};
        QVERIFY(save(db, dmlStatements(target, {update})));
        QCOMPARE(db.value("SELECT v FROM t WHERE b = 2"), QStringLiteral("deux"));

        rows = db.rows("SELECT v, a FROM t");
        QVERIFY(!editTarget(rows, *db.snapshot()).editable());
    }

    void notEditable()
    {
        TestDatabase db;
        db.exec("CREATE TABLE a (id INTEGER PRIMARY KEY); CREATE TABLE b (id INTEGER PRIMARY KEY);"
                "CREATE VIEW v AS SELECT id FROM a; INSERT INTO a VALUES (1); INSERT INTO b VALUES "
                "(1)");
        const auto snapshot = db.snapshot();
        QVERIFY(!editTarget(db.rows("SELECT 1"), *snapshot).editable());
        QVERIFY(!editTarget(db.rows("SELECT a.id, b.id FROM a, b"), *snapshot).editable());
        // Columns of a view come from its table, which is then the target.
        QVERIFY(editTarget(db.rows("SELECT id FROM v"), *snapshot).editable());
    }

    void generatedColumnsAreReadOnly()
    {
        TestDatabase db;
        db.exec("CREATE TABLE t (id INTEGER PRIMARY KEY, a INT, b INT AS (a + 1)); "
                "INSERT INTO t (a) VALUES (1)");
        const EditTarget target = editTarget(db.rows("SELECT * FROM t"), *db.snapshot());
        QVERIFY(target.writable(1));
        QVERIFY(!target.writable(2));
    }

    void typedValuesFollowAffinity()
    {
        QCOMPARE(typedValue(QStringLiteral("42"), Affinity::Integer), Value::integer(42));
        QCOMPARE(typedValue(QStringLiteral("42"), Affinity::Text),
                 Value::text(QStringLiteral("42")));
        QCOMPARE(typedValue(QStringLiteral("abc"), Affinity::Integer),
                 Value::text(QStringLiteral("abc")));
        QCOMPARE(typedValue(QStringLiteral("1.5"), Affinity::Blob).type(),
                 sqliteman::sqlite::ValueType::Real);
    }

    void applyingIsAllOrNothing()
    {
        TestDatabase db;
        db.exec("CREATE TABLE t (id INTEGER PRIMARY KEY, v TEXT UNIQUE); "
                "INSERT INTO t VALUES (1, 'a'), (2, 'b')");
        const auto rows = db.rows("SELECT * FROM t ORDER BY id");
        const EditTarget target = editTarget(rows, *db.snapshot());
        RowEdit first;
        first.key = {rows.valueAt(0, 0)};
        first.values = {{1, Value::text(QStringLiteral("x"))}};
        RowEdit clash; // b becomes x too: breaks UNIQUE.
        clash.key = {rows.valueAt(1, 0)};
        clash.values = {{1, Value::text(QStringLiteral("x"))}};
        const auto statements = dmlStatements(target, {first, clash});
        auto outcome = db.perform([statements](sqlite3 *h) { return applyEdits(h, statements); });
        QVERIFY(!outcome.ok());
        QCOMPARE(db.value("SELECT group_concat(v, ',') FROM t ORDER BY id"), QStringLiteral("a,b"));

        // A row deleted by someone else meanwhile changes nothing: no save.
        RowEdit gone;
        gone.kind = RowEdit::Kind::Delete;
        gone.key = {Value::integer(99)};
        outcome = db.perform(
            [s = dmlStatements(target, {first, gone})](sqlite3 *h) { return applyEdits(h, s); });
        QVERIFY(!outcome.ok());
        QVERIFY(outcome.error.contains(QLatin1String("instead of one")));
        QCOMPARE(db.value("SELECT v FROM t WHERE id = 1"), QStringLiteral("a"));

        outcome = db.perform(
            [s = dmlStatements(target, {first})](sqlite3 *h) { return applyEdits(h, s); });
        QVERIFY2(outcome.ok(), qPrintable(outcome.error));
        QCOMPARE(outcome.task.value.toInt(), 1);
        QCOMPARE(db.value("SELECT v FROM t WHERE id = 1"), QStringLiteral("x"));
        QVERIFY(!db.connection().inTransaction());
    }

    void blobsGoInAsParameters()
    {
        TestDatabase db;
        db.exec(
            "CREATE TABLE t (id INTEGER PRIMARY KEY, data BLOB); INSERT INTO t VALUES (1, NULL)");
        const auto rows = db.rows("SELECT * FROM t");
        const EditTarget target = editTarget(rows, *db.snapshot());
        RowEdit update;
        update.key = {rows.valueAt(0, 0)};
        const QByteArray big(100'000, '\x7f');
        update.values = {{1, Value::blob(big)}};
        const auto statements = dmlStatements(target, {update});
        // The preview is cut short; what runs has it all.
        QVERIFY(statements[0].preview().size() < 300);
        QVERIFY(save(db, statements));
        QCOMPARE(db.value("SELECT length(data) FROM t"), QStringLiteral("100000"));
    }
};

QTEST_GUILESS_MAIN(TestEditing)
#include "tst_editing.moc"
