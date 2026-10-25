// SPDX-FileCopyrightText: 2026 Petr Vanek
// SPDX-License-Identifier: GPL-3.0-or-later

#include "AppTest.h"

#include <QAbstractButton>
#include <QApplication>
#include <QLineEdit>
#include <QMenu>
#include <QSignalSpy>
#include <QTest>
#include <QTimer>
#include <QTreeView>

using namespace sqliteman;
using Node = DatabaseBrowser::Node;

namespace {

QStringList menuTexts(DatabaseBrowser &browser, const QModelIndex &index)
{
    QMenu menu;
    browser.fillContextMenu(menu, index);
    QStringList texts;
    for (const QAction *a : menu.actions())
        if (!a->isSeparator())
            texts << a->text().remove(QLatin1Char('&'));
    return texts;
}

} // namespace

class TestBrowser : public QObject
{
    Q_OBJECT

private:
    QTemporaryDir m_dir;
    QString m_path;

private Q_SLOTS:
    void initTestCase()
    {
        qRegisterMetaType<Session *>();
        m_path = apptest::makeDatabase(
            m_dir, QStringLiteral("browser.db"),
            "CREATE TABLE MixedCase (id INTEGER PRIMARY KEY, Name TEXT);"
            "CREATE INDEX mixed_name ON MixedCase (Name);"
            "CREATE TRIGGER mixed_t AFTER INSERT ON MixedCase BEGIN SELECT 1; END;"
            "CREATE VIEW v AS SELECT Name FROM MixedCase;");
        QVERIFY(!m_path.isEmpty());
    }

    void listsWhatIsInTheDatabase()
    {
        DatabaseBrowser browser;
        Session *session = apptest::open(browser, m_path);
        QVERIFY(session);
        // Issue #10: the table's name as it was written.
        const QModelIndex table
            = browser.indexOf(session, QStringLiteral("main"), QStringLiteral("MixedCase"));
        QVERIFY(table.isValid());
        QCOMPARE(table.data().toString(), QStringLiteral("MixedCase"));
        // Its columns, indexes and triggers under it.
        QCOMPARE(table.model()->rowCount(table), 3);
        QVERIFY(browser.indexOf(session, QStringLiteral("main"), QStringLiteral("mixed_name"))
                    .isValid());
        QVERIFY(
            browser.indexOf(session, QStringLiteral("main"), QStringLiteral("mixed_t")).isValid());
        QVERIFY(browser.indexOf(session, QStringLiteral("main"), QStringLiteral("v")).isValid());
        QVERIFY(browser.indexOf(session, QStringLiteral("main"), QStringLiteral("sqlite_schema"))
                    .isValid());
    }

    void opensEachFileOnce()
    {
        DatabaseBrowser browser;
        Session *first = apptest::open(browser, m_path);
        Session *again = browser.openDatabase(m_path);
        QCOMPARE(again, first);
        QCOMPARE(browser.sessions().size(), 1u);
    }

    void aFileThatIsNoDatabaseIsNotKept()
    {
        const QString path = m_dir.filePath(QStringLiteral("text.txt"));
        QFile file(path);
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write(QByteArray(4096, 'x'));
        file.close();
        DatabaseBrowser browser;
        // The message box it shows would wait for a click.
        QTimer closer;
        connect(&closer, &QTimer::timeout, [] {
            for (QWidget *w : QApplication::topLevelWidgets())
                if (w->inherits("QMessageBox") && w->isVisible())
                    w->close();
        });
        closer.start(50);
        browser.openDatabase(path);
        QTRY_VERIFY(browser.sessions().empty());
    }

    void contextMenusOfferWhatFits()
    {
        DatabaseBrowser browser;
        Session *session = apptest::open(browser, m_path);
        const QStringList table = menuTexts(
            browser, browser.indexOf(session, QStringLiteral("main"), QStringLiteral("MixedCase")));
        for (const char *expected :
             {"Open", "Diagram", "Alter Table…", "Create Index…", "Populate…", "Import CSV…"})
            QVERIFY2(table.contains(QString::fromUtf8(expected)), expected);
        const QStringList view = menuTexts(
            browser, browser.indexOf(session, QStringLiteral("main"), QStringLiteral("v")));
        QVERIFY(view.contains(QStringLiteral("Alter View…")));
        QVERIFY(!view.contains(QStringLiteral("Populate…")));
        QVERIFY(!view.contains(QStringLiteral("Diagram")));
        const QStringList database
            = menuTexts(browser, browser.indexOf(session, QStringLiteral("main")));
        QVERIFY(database.contains(QStringLiteral("Dump Database…")));
        QVERIFY(database.contains(QStringLiteral("Pragmas")));
        QVERIFY(database.contains(QStringLiteral("Details")));
        QVERIFY(database.contains(QStringLiteral("Diagram")));
        // SQLite's own tables are not to be dropped from here.
        const QStringList system = menuTexts(
            browser,
            browser.indexOf(session, QStringLiteral("main"), QStringLiteral("sqlite_schema")));
        QVERIFY(!system.join(QLatin1Char(' ')).contains(QLatin1String("Drop")));
    }

    void followsSchemaChangesWhoeverMakesThem()
    {
        const QString path
            = apptest::makeDatabase(m_dir, QStringLiteral("changes.db"), "CREATE TABLE a (x)");
        DatabaseBrowser browser;
        Session *session = apptest::open(browser, path);
        apptest::run(session, "CREATE TABLE b (y)");
        QTRY_VERIFY(
            browser.indexOf(session, QStringLiteral("main"), QStringLiteral("b")).isValid());
        // A rolled back CREATE disappears again.
        apptest::run(session, "BEGIN; CREATE TABLE c (z)");
        QTRY_VERIFY(
            browser.indexOf(session, QStringLiteral("main"), QStringLiteral("c")).isValid());
        apptest::run(session, "ROLLBACK");
        QTRY_VERIFY(
            !browser.indexOf(session, QStringLiteral("main"), QStringLiteral("c")).isValid());
    }

    void dropsAfterAsking()
    {
        const QString path
            = apptest::makeDatabase(m_dir, QStringLiteral("drop.db"), "CREATE TABLE gone (x)");
        DatabaseBrowser browser;
        Session *session = apptest::open(browser, path);
        bool asked = false;
        browser.setConfirm([&](const QString &) {
            asked = true;
            return true;
        });
        browser.drop(session, catalog::ObjectKind::Table, QStringLiteral("main"),
                     QStringLiteral("gone"));
        QVERIFY(asked);
        QTRY_VERIFY(
            !browser.indexOf(session, QStringLiteral("main"), QStringLiteral("gone")).isValid());
    }

    void attachedDatabasesHaveTheirOwnNode()
    {
        const QString other = apptest::makeDatabase(m_dir, QStringLiteral("other.db"),
                                                    "CREATE TABLE elsewhere (x)");
        DatabaseBrowser browser;
        Session *session = apptest::open(browser, m_path);
        apptest::run(session, QStringLiteral("ATTACH '%1' AS other").arg(other).toUtf8());
        QTRY_VERIFY(browser.indexOf(session, QStringLiteral("other"), QStringLiteral("elsewhere"))
                        .isValid());
        QVERIFY(menuTexts(browser, browser.indexOf(session, QStringLiteral("other")))
                    .contains(QStringLiteral("Detach Database")));
        browser.detach(session, QStringLiteral("other"));
        QTRY_VERIFY(!browser.indexOf(session, QStringLiteral("other")).isValid());
    }

    void filtersByName()
    {
        DatabaseBrowser browser;
        Session *session = apptest::open(browser, m_path);
        browser.filter()->setText(QStringLiteral("mixed_n"));
        QVERIFY(browser.indexOf(session, QStringLiteral("main"), QStringLiteral("mixed_name"))
                    .isValid());
        QVERIFY(!browser.indexOf(session, QStringLiteral("main"), QStringLiteral("v")).isValid());
        browser.filter()->clear();
        QVERIFY(browser.indexOf(session, QStringLiteral("main"), QStringLiteral("v")).isValid());
    }

    void activatingAnObjectOpensIt()
    {
        DatabaseBrowser browser;
        Session *session = apptest::open(browser, m_path);
        QSignalSpy object(&browser, &DatabaseBrowser::objectRequested);
        Q_EMIT browser.view()->activated(
            browser.indexOf(session, QStringLiteral("main"), QStringLiteral("MixedCase")));
        QCOMPARE(object.size(), 1);
        QCOMPARE(object.at(0).at(2).toString(), QStringLiteral("MixedCase"));
        Q_EMIT browser.view()->activated(
            browser.indexOf(session, QStringLiteral("main"), QStringLiteral("mixed_t")));
        QCOMPARE(object.size(), 2);
        // A database expands rather than opening.
        Q_EMIT browser.view()->activated(browser.indexOf(session, QStringLiteral("main")));
        QCOMPARE(object.size(), 2);
    }

    void closingAsksAboutAnOpenTransaction()
    {
        DatabaseBrowser browser;
        Session *session = apptest::open(browser, m_path);
        apptest::run(session, "BEGIN");
        QTRY_VERIFY(session->inTransaction());
        // Roll back, as the box would be answered.
        QTimer answer;
        connect(&answer, &QTimer::timeout, [] {
            for (QWidget *w : QApplication::topLevelWidgets()) {
                if (!w->inherits("QMessageBox") || !w->isVisible())
                    continue;
                for (QAbstractButton *b : w->findChildren<QAbstractButton *>())
                    if (b->text().remove(QLatin1Char('&')) == QLatin1String("Roll Back"))
                        b->click();
            }
        });
        answer.start(50);
        QVERIFY(browser.closeDatabase(session));
        QVERIFY(browser.sessions().empty());
    }
};

QTEST_MAIN(TestBrowser)
#include "tst_browser.moc"
