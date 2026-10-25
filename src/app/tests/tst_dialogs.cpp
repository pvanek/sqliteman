// SPDX-FileCopyrightText: 2026 Petr Vanek
// SPDX-License-Identifier: GPL-3.0-or-later

#include "AppTest.h"

#include "ImportDialog.h"
#include "IndexDialog.h"
#include "MaintenanceDialog.h"
#include "PopulatorDialog.h"
#include "PreferencesDialog.h"
#include "QueryBuilderDialog.h"
#include "SqlEditor.h"
#include "SqlObjectDialog.h"
#include "TableDialog.h"

#include <QApplication>
#include <QDialogButtonBox>
#include <QPushButton>
#include <QSignalSpy>
#include <QTest>
#include <QTimer>

using namespace sqliteman;

class TestDialogs : public QObject
{
    Q_OBJECT

private:
    QTemporaryDir m_dir;
    DatabaseBrowser *m_browser = nullptr;
    Session *m_session = nullptr;

    template<typename Dialog>
    bool apply(Dialog &dialog)
    {
        std::optional<bool> ok;
        dialog.apply([&](bool result) { ok = result; });
        (void)QTest::qWaitFor([&] { return ok.has_value(); }, 10000);
        return ok.value_or(false);
    }

private Q_SLOTS:
    void init()
    {
        m_browser = new DatabaseBrowser;
        const QString path = apptest::makeDatabase(
            m_dir, QStringLiteral("dialogs.db"),
            "CREATE TABLE person (id INTEGER PRIMARY KEY, name TEXT NOT NULL, city TEXT);"
            "CREATE INDEX person_city ON person (city);"
            "INSERT INTO person (name, city) VALUES ('a', 'x'), ('b', 'y');");
        m_session = apptest::open(*m_browser, path);
        QVERIFY(m_session);
    }

    void cleanup()
    {
        delete m_browser;
        m_browser = nullptr;
    }

    void createsATableWithAUniqueConstraint()
    {
        TableDialog dialog(m_session, QStringLiteral("main"));
        QVERIFY(!dialog.isAltering());
        catalog::TableDef t = dialog.definition();
        t.name = QStringLiteral("Visit");
        catalog::ColumnDef who;
        who.name = QStringLiteral("who");
        who.type = QStringLiteral("INTEGER");
        who.extra = QStringLiteral("REFERENCES person (id)");
        catalog::ColumnDef day;
        day.name = QStringLiteral("day");
        day.type = QStringLiteral("TEXT");
        day.defaultValue = QStringLiteral("CURRENT_DATE");
        t.columns.push_back(who);
        t.columns.push_back(day);
        dialog.setDefinition(t);
        // Issue #24: UNIQUE over columns, from the dialog.
        dialog.addColumnsConstraint(QStringLiteral("UNIQUE"), {1, 2});
        QVERIFY(dialog.statement().contains(QLatin1String("UNIQUE (who, day)")));
        QVERIFY(apply(dialog));
        QVERIFY(apptest::waitForSchema(m_session, [](const catalog::Snapshot &s) {
            return s.find(QString(), QStringLiteral("Visit"));
        }));
        apptest::run(m_session, "INSERT INTO Visit (who, day) VALUES (1, '2026-01-01')");
        QVERIFY(
            !apptest::run(m_session, "INSERT INTO Visit (who, day) VALUES (1, '2026-01-01')").ok());
    }

    void altersATable()
    {
        TableDialog dialog(m_session, QStringLiteral("main"), QStringLiteral("person"));
        QVERIFY(dialog.isValid());
        catalog::TableDef t = dialog.definition();
        QCOMPARE(t.columns.size(), 3u);
        t.columns[2].name = QStringLiteral("town"); // A rename,
        t.columns[2].notNull = true; // and a rebuild.
        t.columns[2].defaultValue = QStringLiteral("'?'");
        dialog.setDefinition(t);
        QVERIFY(dialog.statement().contains(QLatin1String("RENAME COLUMN city TO town")));
        QVERIFY(
            dialog.statement().contains(QLatin1String("CREATE TABLE main.sqliteman_new_person")));
        QVERIFY(apply(dialog));
        QCOMPARE(apptest::value(m_session, "SELECT group_concat(town) FROM person"),
                 QStringLiteral("x,y"));
        // The index followed the renamed column.
        QCOMPARE(apptest::value(m_session, "SELECT name FROM pragma_index_info('person_city')"),
                 QStringLiteral("town"));
        QVERIFY(dialog.logText().contains(QLatin1String("Table altered.")));
    }

    void aVirtualTableCannotBeAltered()
    {
        apptest::run(m_session, "CREATE VIRTUAL TABLE docs USING fts5(body)");
        QVERIFY(apptest::waitForSchema(m_session, [](const catalog::Snapshot &s) {
            return s.find(QString(), QStringLiteral("docs"));
        }));
        // The warning it shows: closed as it opens.
        QTimer closer;
        connect(&closer, &QTimer::timeout, [] {
            for (QWidget *w : QApplication::topLevelWidgets())
                if (w->inherits("QMessageBox") && w->isVisible())
                    w->close();
        });
        closer.start(50);
        TableDialog dialog(m_session, QStringLiteral("main"), QStringLiteral("docs"));
        QVERIFY(!dialog.isValid());
    }

    void createsAnIndex()
    {
        IndexDialog dialog(m_session, QStringLiteral("main"), QStringLiteral("person"));
        dialog.setName(QStringLiteral("person_name"));
        QVERIFY(dialog.useColumn(QStringLiteral("name"), true));
        QVERIFY(!dialog.useColumn(QStringLiteral("nothing")));
        QCOMPARE(dialog.statement(),
                 QStringLiteral("CREATE INDEX main.person_name ON person (name DESC)"));
        QVERIFY(apply(dialog));
        QCOMPARE(apptest::value(m_session,
                                "SELECT desc FROM pragma_index_xinfo('person_name') WHERE cid = 1"),
                 QStringLiteral("1"));
    }

    void viewsAndTriggers()
    {
        SqlObjectDialog create(m_session, SqlObjectDialog::Kind::View, QStringLiteral("main"),
                               QString(), QString());
        create.setName(QStringLiteral("names"));
        create.setViewQuery(QStringLiteral("SELECT name FROM person;"));
        QVERIFY(apply(create));
        QVERIFY(apptest::waitForSchema(m_session, [](const catalog::Snapshot &s) {
            return s.find(QString(), QStringLiteral("names"));
        }));

        // Altering keeps the old one when the new one is wrong.
        SqlObjectDialog broken(m_session, SqlObjectDialog::Kind::View, QStringLiteral("main"),
                               QStringLiteral("names"), QString());
        QCOMPARE(broken.editor()->text(), QStringLiteral("SELECT name FROM person"));
        broken.setViewQuery(QStringLiteral("SELECT nothing FROM nowhere"));
        QVERIFY(!apply(broken));
        QVERIFY(apptest::run(m_session, "SELECT * FROM names").ok());

        SqlObjectDialog altered(m_session, SqlObjectDialog::Kind::View, QStringLiteral("main"),
                                QStringLiteral("names"), QString());
        altered.setViewQuery(QStringLiteral("SELECT upper(name) AS name FROM person"));
        QVERIFY(apply(altered));
        QCOMPARE(apptest::value(m_session, "SELECT name FROM names ORDER BY 1"),
                 QStringLiteral("A"));

        SqlObjectDialog trigger(m_session, SqlObjectDialog::Kind::Trigger, QStringLiteral("main"),
                                QString(), QStringLiteral("person"));
        trigger.setTriggerTemplate(false);
        QVERIFY(apply(trigger)); // The template works as it is.
    }

    void populates()
    {
        PopulatorDialog dialog(m_session, QStringLiteral("main"), QStringLiteral("person"));
        QVERIFY(dialog.isValid());
        dialog.setCount(25);
        QVERIFY(apply(dialog));
        QCOMPARE(apptest::value(m_session, "SELECT count(*) FROM person"), QStringLiteral("27"));
        QCOMPARE(apptest::value(m_session, "SELECT max(id) FROM person"), QStringLiteral("27"));
    }

    void maintenance()
    {
        MaintenanceDialog dialog(m_session, QStringLiteral("main"));
        bool done = false;
        dialog.run(QStringLiteral("PRAGMA integrity_check"), QStringLiteral("Integrity check"),
                   [&] { done = true; });
        QTRY_VERIFY(done);
        QVERIFY(dialog.logText().contains(QLatin1String("ok")));
    }

    void buildsAQuery()
    {
        QueryBuilderDialog dialog(m_session->snapshot());
        dialog.setTable(QStringLiteral("main"), QStringLiteral("person"));
        dialog.selectColumns({QStringLiteral("name")});
        dialog.addTerm(QStringLiteral("city"), catalog::QuerySpec::Relation::Equals,
                       QStringLiteral("x"));
        QCOMPARE(dialog.statement(), QStringLiteral("SELECT name\nFROM person\nWHERE city = 'x';"));
        QCOMPARE(apptest::value(m_session, dialog.statement().toUtf8()), QStringLiteral("a"));
    }

    void importsCsv()
    {
        const QString csv = m_dir.filePath(QStringLiteral("people.csv"));
        QFile file(csv);
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write("name;city\nc;z\nd;w\n");
        file.close();
        ImportDialog dialog(m_session, QStringLiteral("main"), QStringLiteral("person"));
        dialog.setPath(csv); // Guesses ; as the delimiter, matches columns by name.
        const catalog::ImportPlan plan = dialog.plan();
        QCOMPARE(plan.columns.size(), 2u);
        QCOMPARE(plan.columns[0].name, QStringLiteral("name"));
        QSignalSpy imported(&dialog, &ImportDialog::imported);
        dialog.show();
        dialog.findChild<QDialogButtonBox *>()->button(QDialogButtonBox::Ok)->click();
        QTRY_COMPARE(imported.size(), 1);
        QCOMPARE(apptest::value(m_session, "SELECT count(*) FROM person"), QStringLiteral("4"));
    }

    void preferencesRoundTrip()
    {
        config::Preferences p;
        p.nullText = QStringLiteral("∅");
        p.rowLimit = 1234;
        p.snippets = {{QStringLiteral("x"), QStringLiteral("SELECT\n1")}};
        p.allowExtensions = true;
        p.startupExtensions = {QStringLiteral("/tmp/ext.so")};
        PreferencesDialog dialog(p);
        const config::Preferences back = dialog.preferences();
        QCOMPARE(back.nullText, p.nullText);
        QCOMPARE(back.rowLimit, 1234);
        QCOMPARE(back.snippets, p.snippets); // A line break survives the one-line cell.
        QCOMPARE(back.startupExtensions, p.startupExtensions);
    }
};

QTEST_MAIN(TestDialogs)
#include "tst_dialogs.moc"
