// SPDX-FileCopyrightText: 2026 Petr Vanek
// SPDX-License-Identifier: GPL-3.0-or-later

#include "AppTest.h"

#include "EditorPage.h"
#include "MainWindow.h"
#include "ResultModel.h"
#include "ResultView.h"
#include "ErdView.h"
#include "ObjectPage.h"
#include "Shortcuts.h"
#include "SqlEditor.h"
#include "TableData.h"

#include <QLineEdit>
#include <QTabWidget>
#include <QTableWidget>
#include <QTest>

using namespace sqliteman;

class TestMainWindow : public QObject
{
    Q_OBJECT

private:
    QTemporaryDir m_dir;
    QString m_path;

private Q_SLOTS:
    void init()
    {
        m_path = apptest::makeDatabase(m_dir, QStringLiteral("main.db"),
                                       "CREATE TABLE plain (a, b);"
                                       "INSERT INTO plain VALUES (1, 'x'), (2, 'y'), (3, 'z');"
                                       "CREATE TABLE keyed (k TEXT PRIMARY KEY, v) WITHOUT ROWID;"
                                       "INSERT INTO keyed VALUES ('one', 1), ('two', 2);"
                                       "CREATE TABLE child (id INTEGER PRIMARY KEY, "
                                       "k TEXT NOT NULL REFERENCES keyed);"
                                       "CREATE VIEW v AS SELECT a FROM plain;"
                                       "CREATE INDEX plain_a ON plain (a);");
        QVERIFY(!m_path.isEmpty());
    }

    void opensDatabasesAndScriptsByWhatTheyAre()
    {
        const QString script = m_dir.filePath(QStringLiteral("query.db.sql"));
        QFile file(script);
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write("SELECT 1;\n");
        file.close();
        QVERIFY(MainWindow::isDatabaseFile(m_path));
        QVERIFY(!MainWindow::isDatabaseFile(script));

        MainWindow window;
        window.open(m_path);
        QTRY_COMPARE(window.browser()->sessions().size(), 1u);
        window.open(script);
        EditorPage *editor = window.currentEditor();
        QVERIFY(editor);
        QCOMPARE(editor->editor()->text(), QStringLiteral("SELECT 1;\n"));
        QVERIFY(window.closePage(editor));
    }

    void opensAnInMemoryDatabase()
    {
        MainWindow window;
        window.open(QStringLiteral(":memory:"));
        QTRY_COMPARE(window.browser()->sessions().size(), 1u);
        Session *session = window.browser()->sessions().front();
        QTRY_VERIFY(session->isOpen());
        QCOMPARE(apptest::value(session, "SELECT 6 * 7"), QStringLiteral("42"));
    }

    ObjectPage *table(MainWindow &window, Session *session, const char *name)
    {
        return window.showObject(session, QStringLiteral("main"), QString::fromUtf8(name));
    }

    void reusesTheTablesPage()
    {
        MainWindow window;
        Session *session = apptest::open(*window.browser(), m_path);
        QVERIFY(session);
        ObjectPage *first = table(window, session, "plain");
        ObjectPage *again = table(window, session, "plain");
        QCOMPARE(again, first);
        QCOMPARE(window.currentPage(), first);
    }

    // A table's or view's page opens at its rows, which are a tab of it;
    // an index has no rows, and opens at its SQL.
    void tablesAndViewsOpenAtTheirRows()
    {
        MainWindow window;
        Session *session = apptest::open(*window.browser(), m_path);
        ObjectPage *page = table(window, session, "plain");
        QVERIFY(page->data());
        QCOMPARE(page->tabs()->currentWidget(), page->data());
        QVERIFY(page->diagram());
        ObjectPage *view = table(window, session, "v");
        QVERIFY(view->data());
        QVERIFY(!view->diagram()); // A view has no foreign keys.
        QTRY_COMPARE(view->data()->results()->model()->rowCount(), 3);
        ObjectPage *index = table(window, session, "plain_a");
        QVERIFY(!index->data());
        QVERIFY(!index->diagram());
        QTRY_COMPARE(index->details().object.kind, catalog::ObjectKind::Index);
        QCOMPARE(index->tabs()->currentWidget(), index->sql());
    }

    void editsATableByItsRowid()
    {
        MainWindow window;
        Session *session = apptest::open(*window.browser(), m_path);
        TableData *page = table(window, session, "plain")->data();
        // A table without a key of its own: the rowid finds its rows.
        QVERIFY(page->query().contains("rowid"));
        ResultModel *model = page->results()->model();
        QTRY_COMPARE(model->rowCount(), 3);
        QTRY_VERIFY(model->isEditable());
        const int b = model->columnCount() - 1;
        QVERIFY(model->setData(model->index(1, b), QStringLiteral("Y")));
        model->toggleDeleted({2});
        page->saveChanges(false);
        QTRY_COMPARE(apptest::value(session, "SELECT group_concat(b, '') FROM plain"),
                     QStringLiteral("xY"));
    }

    void filtersAndOrdersTheRows()
    {
        MainWindow window;
        Session *session = apptest::open(*window.browser(), m_path);
        TableData *page = table(window, session, "keyed")->data();
        // No rowid in a WITHOUT ROWID table: its key finds the rows.
        QVERIFY(!page->query().contains("rowid"));
        page->filter()->setText(QStringLiteral("v > 0"));
        page->order()->setText(QStringLiteral("k DESC"));
        QVERIFY(page->query().contains("WHERE v > 0"));
        page->refresh();
        ResultModel *model = page->results()->model();
        QTRY_COMPARE(model->rowCount(), 2);
        QTRY_COMPARE(model->data(model->index(0, 0)).toString(), QStringLiteral("two"));
        QTRY_VERIFY(model->isEditable());
    }

    // The rows' refresh and the page's are the same keys: only one may be on.
    void refreshIsTheRowsOnTheirTab()
    {
        MainWindow window;
        Session *session = apptest::open(*window.browser(), m_path);
        ObjectPage *page = table(window, session, "plain");
        const auto pageRefresh = [page] {
            for (QAction *a : page->actions()) {
                if (a->shortcuts() == Shortcuts::refresh())
                    return a;
            }
            return static_cast<QAction *>(nullptr);
        };
        QVERIFY(pageRefresh());
        QVERIFY(!pageRefresh()->isEnabled());
        page->tabs()->setCurrentWidget(page->columns());
        QVERIFY(pageRefresh()->isEnabled());
    }

    void theDatabaseHasAPage()
    {
        MainWindow window;
        Session *session = apptest::open(*window.browser(), m_path);
        ObjectPage *page = window.showObject(session, QStringLiteral("main"), QString());
        QVERIFY(page->isDatabase());
        QVERIFY(!page->data());
        QCOMPARE(page->title(), session->name());
        QTRY_VERIFY(page->objects()->rowCount() > 0);
        QCOMPARE(page->objects()->rowCount(), 5); // plain, keyed, child, v and plain_a.
        QVERIFY(page->properties()->rowCount() > 10);
        QVERIFY(page->sql()->text().contains(QLatin1String("CREATE VIEW v")));

        // An object of the list opens its own page.
        int row = 0;
        while (page->objects()->item(row, 0)->text() != QLatin1String("keyed"))
            ++row;
        Q_EMIT page->objects()->cellActivated(row, 0);
        auto *keyed = qobject_cast<ObjectPage *>(window.currentPage());
        QVERIFY(keyed);
        QCOMPARE(keyed->name(), QStringLiteral("keyed"));

        // It follows the schema.
        apptest::run(session, "CREATE TABLE later (x)");
        QTRY_COMPARE(page->objects()->rowCount(), 6);
    }

    void diagramsOfATableAndOfTheDatabase()
    {
        MainWindow window;
        Session *session = apptest::open(*window.browser(), m_path);
        Q_EMIT window.browser()->diagramRequested(session, QStringLiteral("main"),
                                                  QStringLiteral("child"));
        auto *page = qobject_cast<ObjectPage *>(window.currentPage());
        QVERIFY(page && page->diagram());
        QVERIFY(page->tabs()->currentWidget()->isAncestorOf(page->diagram()));
        // child and what it references.
        QCOMPARE(page->diagram()->graph().nodes.size(), 2u);
        QCOMPARE(page->diagram()->graph().edges.size(), 1u);

        // Double-clicking the other table opens its page.
        const catalog::ErdNode &keyed = page->diagram()->graph().nodes.back();
        QCOMPARE(keyed.name, QStringLiteral("keyed"));
        Q_EMIT page->diagram()->tableActivated(keyed.id);
        auto *opened = qobject_cast<ObjectPage *>(window.currentPage());
        QVERIFY(opened && opened != page);
        QCOMPARE(opened->name(), QStringLiteral("keyed"));

        ObjectPage *database = window.showObject(session, QStringLiteral("main"), QString());
        database->showDiagram();
        QCOMPARE(database->diagram()->graph().nodes.size(), 3u); // Every table, not the view.

        // The diagram follows the schema, too.
        apptest::run(session, "CREATE TABLE grandchild (c REFERENCES child)");
        QTRY_COMPARE(database->diagram()->graph().nodes.size(), 4u);
    }

    void closingTheDatabaseClosesItsPages()
    {
        MainWindow window;
        Session *session = apptest::open(*window.browser(), m_path);
        table(window, session, "plain");
        EditorPage *editor = window.newEditor(session, QStringLiteral("SELECT 1"));
        editor->editor()->setModified(false);
        const auto before = window.pages().size();
        QVERIFY(window.browser()->closeDatabase(session));
        QTRY_VERIFY(window.pages().size() < before);
        // The editor stays, without a database; the table's page goes.
        for (WorkspacePage *p : window.pages())
            QVERIFY(!qobject_cast<ObjectPage *>(p));
    }
};

QTEST_MAIN(TestMainWindow)
#include "tst_mainwindow.moc"
