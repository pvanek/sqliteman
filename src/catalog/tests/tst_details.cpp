// SPDX-FileCopyrightText: 2026 Petr Vanek
// SPDX-License-Identifier: GPL-3.0-or-later

#include "TestDatabase.h"

#include "catalog/Details.h"

#include <QFileInfo>
#include <QTest>

using namespace sqliteman::catalog;

class TestDetails : public QObject
{
    Q_OBJECT

private:
    ObjectDetails details(TestDatabase &db, const QString &schema, const QString &name)
    {
        const auto outcome = db.perform([schema, name](sqlite3 *handle) {
            sqliteman::sqlite::TaskOutcome o;
            o.value = QVariant::fromValue(loadDetails(handle, schema, name));
            return o;
        });
        return outcome.task.value.value<ObjectDetails>();
    }

private Q_SLOTS:
    void table()
    {
        TestDatabase db;
        db.exec(
            "CREATE TABLE parent (id INTEGER PRIMARY KEY, a, b, UNIQUE (a, b));"
            "CREATE TABLE child (id INTEGER PRIMARY KEY, pa, pb, code TEXT UNIQUE, "
            "FOREIGN KEY (pa, pb) REFERENCES parent (a, b) ON DELETE CASCADE);"
            "CREATE INDEX child_code ON child (code COLLATE NOCASE DESC) WHERE code IS NOT NULL;"
            "CREATE TRIGGER child_t AFTER DELETE ON child BEGIN SELECT 1; END;");
        const ObjectDetails d = details(db, QStringLiteral("main"), QStringLiteral("child"));
        QVERIFY2(d.error.isEmpty(), qPrintable(d.error));
        QCOMPARE(d.object.kind, ObjectKind::Table);
        QCOMPARE(d.indexes.size(), 2u);
        const IndexDetails &created
            = d.indexes[0].name == QLatin1String("child_code") ? d.indexes[0] : d.indexes[1];
        const IndexDetails &unique = &created == &d.indexes[0] ? d.indexes[1] : d.indexes[0];
        QCOMPARE(created.columns, QStringList {QStringLiteral("code COLLATE NOCASE DESC")});
        QVERIFY(created.partial);
        QVERIFY(!created.sql.isEmpty());
        QCOMPARE(unique.origin, IndexDetails::Origin::Unique);
        QVERIFY(unique.unique);
        QVERIFY(unique.sql.isEmpty());

        QCOMPARE(d.foreignKeys.size(), 1u);
        QCOMPARE(d.foreignKeys[0].table, QStringLiteral("parent"));
        QCOMPARE(d.foreignKeys[0].from, (QStringList {QStringLiteral("pa"), QStringLiteral("pb")}));
        QCOMPARE(d.foreignKeys[0].to, (QStringList {QStringLiteral("a"), QStringLiteral("b")}));
        QCOMPARE(d.foreignKeys[0].onDelete, QStringLiteral("CASCADE"));
        QCOMPARE(d.triggers.size(), 1u);
    }

    void index()
    {
        TestDatabase db;
        db.exec("CREATE TABLE t (a, b); CREATE UNIQUE INDEX t_ab ON t (a, b DESC)");
        const ObjectDetails d = details(db, QStringLiteral("main"), QStringLiteral("t_ab"));
        QCOMPARE(d.object.kind, ObjectKind::Index);
        QCOMPARE(d.indexes.size(), 1u);
        QVERIFY(d.indexes[0].unique);
        QCOMPARE(d.indexes[0].columns,
                 (QStringList {QStringLiteral("a"), QStringLiteral("b DESC")}));
    }

    void missing()
    {
        TestDatabase db;
        QVERIFY(!details(db, QStringLiteral("main"), QStringLiteral("nothing")).error.isEmpty());
    }

    void database()
    {
        TestDatabase db;
        db.exec("PRAGMA user_version = 7; CREATE TABLE t (a); CREATE VIEW v AS SELECT a FROM t;"
                "CREATE INDEX t_a ON t (a); CREATE TABLE gone (b); DROP TABLE gone");
        const auto outcome = db.perform([](sqlite3 *handle) {
            sqliteman::sqlite::TaskOutcome o;
            o.value = QVariant::fromValue(loadDatabaseDetails(handle, QStringLiteral("main")));
            return o;
        });
        const DatabaseDetails d = outcome.task.value.value<DatabaseDetails>();
        QVERIFY2(d.error.isEmpty(), qPrintable(d.error));
        QCOMPARE(d.database.name, QStringLiteral("main"));
        QCOMPARE(d.database.objects.size(), 3u);
        const auto property = [&d](const char *name) {
            for (const auto &[key, value] : d.properties) {
                if (key == QLatin1String(name))
                    return value;
            }
            return QStringLiteral("<missing>");
        };
        // The same file, not the same string: on Windows SQLite gives the
        // path with backslashes, and maybe a short name where Qt has the long.
        QVERIFY2(QFileInfo(property("File")) == QFileInfo(db.path()),
                 qPrintable(property("File") + QStringLiteral(" is not ") + db.path()));
        QCOMPARE(property("User version"), QStringLiteral("7"));
        QCOMPARE(property("Tables"), QStringLiteral("1"));
        QCOMPARE(property("Views"), QStringLiteral("1"));
        QCOMPARE(property("Indexes"), QStringLiteral("1"));
        QCOMPARE(property("Encoding"), QStringLiteral("UTF-8"));
        // The dropped table left a free page behind.
        QVERIFY(property("Free pages").contains(QLatin1String("VACUUM")));

        const auto missing = db.perform([](sqlite3 *handle) {
            sqliteman::sqlite::TaskOutcome o;
            o.value = QVariant::fromValue(loadDatabaseDetails(handle, QStringLiteral("nowhere")));
            return o;
        });
        QVERIFY(!missing.task.value.value<DatabaseDetails>().error.isEmpty());
    }

    void statements()
    {
        QCOMPARE(dropStatement(ObjectKind::View, QStringLiteral("main"), QStringLiteral("My View")),
                 QStringLiteral("DROP VIEW main.\"My View\""));
        QCOMPARE(dropStatement(ObjectKind::Database, QString(), QStringLiteral("other")),
                 QStringLiteral("DETACH DATABASE other"));
        QCOMPARE(createIndexStatement(QStringLiteral("aux"), QStringLiteral("ix"),
                                      QStringLiteral("t"),
                                      {QStringLiteral("a"), QStringLiteral("b DESC")}, true, false,
                                      QStringLiteral("a > 0")),
                 QStringLiteral("CREATE UNIQUE INDEX aux.ix ON t (a, b DESC) WHERE a > 0"));
        QCOMPARE(createViewStatement(QStringLiteral("main"), QStringLiteral("v"),
                                     QStringLiteral("SELECT 1;\n")),
                 QStringLiteral("CREATE VIEW main.v AS\nSELECT 1"));

        // The trigger template is valid SQL as it is.
        TestDatabase db;
        db.exec("CREATE TABLE t (a); CREATE VIEW v AS SELECT a FROM t");
        db.exec(triggerTemplate(QStringLiteral("main"), QStringLiteral("t"), false).toUtf8());
        db.exec(triggerTemplate(QStringLiteral("main"), QStringLiteral("v"), true).toUtf8());
    }
};

QTEST_GUILESS_MAIN(TestDetails)
#include "tst_details.moc"
