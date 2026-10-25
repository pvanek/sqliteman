// SPDX-FileCopyrightText: 2026 Petr Vanek
// SPDX-License-Identifier: GPL-3.0-or-later

#include "TestDatabase.h"

#include "catalog/TableDefinition.h"

#include <QTest>

using namespace sqliteman::catalog;

class TestTableDefinition : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void parsesColumnsAndConstraints()
    {
        const auto t = parseCreateTable(
            QStringLiteral("CREATE TABLE \"Order Items\" (\n"
                           "  id INTEGER PRIMARY KEY AUTOINCREMENT,\n"
                           "  name VARCHAR(20) NOT NULL DEFAULT 'x' COLLATE NOCASE,\n"
                           "  price DECIMAL(10, 2) CHECK (price >= 0),\n"
                           "  owner INTEGER REFERENCES person(id) ON DELETE SET NULL,\n"
                           "  code TEXT UNIQUE,\n"
                           "  untyped,\n"
                           "  CONSTRAINT one_name UNIQUE (name, owner)\n"
                           ") WITHOUT ROWID, STRICT"));
        QVERIFY(t.has_value());
        QCOMPARE(t->name, QStringLiteral("Order Items"));
        QVERIFY(t->withoutRowid);
        QVERIFY(t->strict);
        QCOMPARE(t->columns.size(), 6u);

        const ColumnDef &id = t->columns[0];
        QCOMPARE(id.type, QStringLiteral("INTEGER"));
        QVERIFY(id.primaryKey);
        QVERIFY(id.autoincrement);

        const ColumnDef &name = t->columns[1];
        QCOMPARE(name.type, QStringLiteral("VARCHAR(20)"));
        QVERIFY(name.notNull);
        QCOMPARE(name.defaultValue, QStringLiteral("'x'"));
        QCOMPARE(name.collate, QStringLiteral("NOCASE"));

        QCOMPARE(t->columns[2].type, QStringLiteral("DECIMAL(10, 2)"));
        QCOMPARE(t->columns[2].check, QStringLiteral("price >= 0"));
        // A foreign key, SET NULL included, stays as it was written.
        QCOMPARE(t->columns[3].extra, QStringLiteral("REFERENCES person(id) ON DELETE SET NULL"));
        QVERIFY(!t->columns[3].notNull);
        QVERIFY(t->columns[4].unique);
        QVERIFY(t->columns[5].type.isEmpty());
        QCOMPARE(t->constraints,
                 QStringList {QStringLiteral("CONSTRAINT one_name UNIQUE (name, owner)")});
    }

    void keepsWhatItDoesNotKnow()
    {
        const auto t = parseCreateTable(QStringLiteral(
            "CREATE TABLE t (a INTEGER PRIMARY KEY DESC ON CONFLICT REPLACE, "
            "b TEXT NOT NULL ON CONFLICT IGNORE, c INT GENERATED ALWAYS AS (a * 2) STORED, "
            "d INT AS (a + 1), e TEXT DEFAULT (datetime('now')), f INT DEFAULT -1, "
            "g REFERENCES u NOT DEFERRABLE)"));
        QVERIFY(t.has_value());
        QCOMPARE(t->columns[0].extra, QStringLiteral("PRIMARY KEY DESC ON CONFLICT REPLACE"));
        QVERIFY(!t->columns[0].primaryKey);
        QCOMPARE(t->columns[1].extra, QStringLiteral("NOT NULL ON CONFLICT IGNORE"));
        QCOMPARE(t->columns[2].extra, QStringLiteral("GENERATED ALWAYS AS (a * 2) STORED"));
        QCOMPARE(t->columns[3].extra, QStringLiteral("AS (a + 1)"));
        QCOMPARE(t->columns[4].defaultValue, QStringLiteral("(datetime('now'))"));
        QCOMPARE(t->columns[5].defaultValue, QStringLiteral("-1"));
        QCOMPARE(t->columns[6].extra, QStringLiteral("REFERENCES u NOT DEFERRABLE"));
    }

    void notATableWithColumns()
    {
        QVERIFY(!parseCreateTable(QStringLiteral("CREATE TABLE t AS SELECT 1")));
        QVERIFY(!parseCreateTable(QStringLiteral("CREATE VIRTUAL TABLE t USING fts5(a)")));
        QVERIFY(!parseCreateTable(QStringLiteral("CREATE VIEW v AS SELECT 1")));
        QVERIFY(!parseCreateTable(QStringLiteral("CREATE TABLE t (")));
    }

    void buildsWhatSqliteTakes()
    {
        TableDef t;
        t.schema = QStringLiteral("main");
        t.name = QStringLiteral("CamelCase");
        ColumnDef id;
        id.name = QStringLiteral("id");
        id.type = QStringLiteral("INTEGER");
        id.primaryKey = true;
        id.autoincrement = true;
        ColumnDef name;
        name.name = QStringLiteral("Full Name");
        name.type = QStringLiteral("TEXT");
        name.notNull = true;
        name.defaultValue = defaultExpression(QStringLiteral("nobody's"));
        ColumnDef order;
        order.name = QStringLiteral("order"); // A keyword: quoted.
        order.type = QStringLiteral("INT");
        order.check = QStringLiteral("\"order\" > 0");
        t.columns = {id, name, order};
        // Issue #24: a UNIQUE constraint from a list of columns.
        t.constraints << columnsConstraint(QStringLiteral("UNIQUE"),
                                           {QStringLiteral("Full Name"), QStringLiteral("order")});

        const QString sql = createTableStatement(t);
        QCOMPARE(sql,
                 QStringLiteral("CREATE TABLE main.CamelCase (\n"
                                "    id INTEGER PRIMARY KEY AUTOINCREMENT,\n"
                                "    \"Full Name\" TEXT NOT NULL DEFAULT 'nobody''s',\n"
                                "    \"order\" INT CHECK (\"order\" > 0),\n"
                                "    UNIQUE (\"Full Name\", \"order\")\n"
                                ")"));

        TestDatabase db;
        db.exec(sql.toUtf8());
        db.exec("INSERT INTO CamelCase (\"Full Name\", \"order\") VALUES ('a', 1)");
        QVERIFY(!db.run("INSERT INTO CamelCase (\"Full Name\", \"order\") VALUES ('a', 1)").ok());
        QCOMPARE(db.value("SELECT \"Full Name\" FROM CamelCase WHERE \"order\" = 1"),
                 QStringLiteral("a"));
        // What SQLite stored parses back to the same table.
        const auto parsed
            = parseCreateTable(db.value("SELECT sql FROM sqlite_schema WHERE name = 'CamelCase'"));
        QVERIFY(parsed.has_value());
        QCOMPARE(parsed->columns, t.columns);
        QCOMPARE(parsed->constraints, t.constraints);
    }

    void defaults_data()
    {
        QTest::addColumn<QString>("typed");
        QTest::addColumn<QString>("expression");
        QTest::newRow("empty") << QString() << QString();
        QTest::newRow("integer") << QStringLiteral("42") << QStringLiteral("42");
        QTest::newRow("negative") << QStringLiteral("-1.5") << QStringLiteral("-1.5");
        QTest::newRow("text") << QStringLiteral("abc") << QStringLiteral("'abc'");
        QTest::newRow("quote inside") << QStringLiteral("it's") << QStringLiteral("'it''s'");
        QTest::newRow("quoted") << QStringLiteral("'abc'") << QStringLiteral("'abc'");
        QTest::newRow("expression") << QStringLiteral("(1 + 2)") << QStringLiteral("(1 + 2)");
        QTest::newRow("keyword") << QStringLiteral("current_timestamp")
                                 << QStringLiteral("current_timestamp");
        QTest::newRow("null") << QStringLiteral("NULL") << QStringLiteral("NULL");
        QTest::newRow("blob") << QStringLiteral("x'00'") << QStringLiteral("x'00'");
        QTest::newRow("number and words") << QStringLiteral("1 day") << QStringLiteral("'1 day'");
    }

    void defaults()
    {
        QFETCH(QString, typed);
        QFETCH(QString, expression);
        QCOMPARE(defaultExpression(typed), expression);
    }
};

QTEST_GUILESS_MAIN(TestTableDefinition)
#include "tst_tabledefinition.moc"
