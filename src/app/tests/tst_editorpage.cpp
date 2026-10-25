// SPDX-FileCopyrightText: 2026 Petr Vanek
// SPDX-License-Identifier: GPL-3.0-or-later

#include "AppTest.h"

#include "EditorPage.h"
#include "PlanView.h"
#include "ResultModel.h"
#include "ResultPanel.h"
#include "ResultView.h"
#include "Settings.h"
#include "SqlEditor.h"

#include <QLabel>
#include <QPlainTextEdit>
#include <QTest>

using namespace sqliteman;

class TestEditorPage : public QObject
{
    Q_OBJECT

private:
    QTemporaryDir m_dir;
    DatabaseBrowser *m_browser = nullptr;
    Session *m_session = nullptr;

    void runAndWait(EditorPage &page, const QString &sql, qsizetype cursor = 0)
    {
        page.editor()->setText(sql);
        page.editor()->setCursorPosition(cursor);
        page.run();
        QTRY_VERIFY(!page.isRunning());
    }

private Q_SLOTS:
    void init()
    {
        m_browser = new DatabaseBrowser;
        const QString path
            = apptest::makeDatabase(m_dir, QStringLiteral("editor.db"),
                                    "CREATE TABLE t (id INTEGER PRIMARY KEY, name TEXT); "
                                    "INSERT INTO t (name) VALUES ('a'), ('b'), ('c');");
        m_session = apptest::open(*m_browser, path);
        QVERIFY(m_session);
    }

    void cleanup()
    {
        delete m_browser;
        m_browser = nullptr;
    }

    void runsAndShowsRows()
    {
        EditorPage page(m_browser, QStringLiteral("Script"));
        page.setSession(m_session);
        runAndWait(page, QStringLiteral("SELECT * FROM t ORDER BY id;"));
        ResultModel *model = page.resultPanel()->results()->model();
        QCOMPARE(model->rowCount(), 3);
        // A query of one table with its key: the rows can be edited.
        QTRY_VERIFY(model->isEditable());
        QVERIFY(page.resultPanel()->messages()->toPlainText().contains(QLatin1String("3 row(s)")));
    }

    void marksTheErrorWhereSqliteSaysItIs()
    {
        EditorPage page(m_browser, QStringLiteral("Script"));
        page.setSession(m_session);
        const QString sql = QStringLiteral("SELECT 1;\nSELECT nothing FROM t;");
        runAndWait(page, sql, 12);
        const qsizetype at = sql.toUtf8().indexOf("nothing");
        QVERIFY(page.editor()->hasErrorAt(at));
        // And for a missing table, where SQLite says no position, by its name.
        const QString missing = QStringLiteral("SELECT * FROM nowhere;");
        runAndWait(page, missing);
        QVERIFY(page.editor()->hasErrorAt(missing.toUtf8().indexOf("nowhere")));
    }

    void runsAScriptAndStopsAtAnError()
    {
        EditorPage page(m_browser, QStringLiteral("Script"));
        page.setSession(m_session);
        page.editor()->setText(QStringLiteral("INSERT INTO t (name) VALUES ('d');\n"
                                              ".headers on\n"
                                              "INSERT INTO nowhere VALUES (1);\n"
                                              "INSERT INTO t (name) VALUES ('e');"));
        page.runScript();
        QTRY_VERIFY(!page.isRunning());
        QCOMPARE(apptest::value(m_session, "SELECT group_concat(name, '') FROM t"),
                 QStringLiteral("abcd"));
        QVERIFY(
            page.resultPanel()->messages()->toPlainText().contains(QLatin1String("sqlite3 shell")));
    }

    void explainsTheQueryPlan()
    {
        EditorPage page(m_browser, QStringLiteral("Script"));
        page.setSession(m_session);
        page.editor()->setText(QStringLiteral("SELECT * FROM t WHERE id = 2"));
        page.explain();
        QTRY_VERIFY(!page.isRunning());
        QVERIFY(page.resultPanel()->plan()->hasPlan());
        QCOMPARE(page.resultPanel()->plan()->plan().steps.front().kind,
                 catalog::PlanStep::Kind::Search);
    }

    void transactionsAreTheDatabases()
    {
        EditorPage page(m_browser, QStringLiteral("Script"));
        page.setSession(m_session);
        runAndWait(page, QStringLiteral("BEGIN; DELETE FROM t;"));
        QTRY_VERIFY(m_session->inTransaction());
        QVERIFY(page.transactionIndicator()->isEnabled());
        page.rollback();
        QTRY_VERIFY(!m_session->inTransaction());
        QCOMPARE(apptest::value(m_session, "SELECT count(*) FROM t"), QStringLiteral("3"));
    }

    void beginsATransaction()
    {
        EditorPage page(m_browser, QStringLiteral("Script"));
        page.setSession(m_session);
        QTRY_VERIFY(m_session->isOpen());
        page.begin();
        QTRY_VERIFY(m_session->inTransaction());
        // Once open, Begin has nothing more to do; Commit ends it.
        page.begin();
        QTRY_VERIFY(!page.isRunning());
        QVERIFY(m_session->inTransaction());
        runAndWait(page, QStringLiteral("DELETE FROM t WHERE id = 1"));
        page.commit();
        QTRY_VERIFY(!m_session->inTransaction());
        QCOMPARE(apptest::value(m_session, "SELECT count(*) FROM t"), QStringLiteral("2"));
    }

    void savesEditedRows()
    {
        EditorPage page(m_browser, QStringLiteral("Script"));
        page.setSession(m_session);
        runAndWait(page, QStringLiteral("SELECT id, name FROM t ORDER BY id"));
        ResultView *view = page.resultPanel()->results();
        QTRY_VERIFY(view->model()->isEditable());
        view->model()->setData(view->model()->index(1, 1), QStringLiteral("B"));
        page.saveChanges(view, false);
        QTRY_COMPARE(apptest::value(m_session, "SELECT name FROM t WHERE id = 2"),
                     QStringLiteral("B"));
        // The query ran again: the grid shows what is saved, nothing pending.
        QTRY_VERIFY(!view->model()->hasChanges());
    }

    void stopsAtTheRowLimit()
    {
        config::Preferences p = Settings::instance().preferences();
        p.rowLimit = 100;
        Settings::instance().setPreferences(p);
        EditorPage page(m_browser, QStringLiteral("Script"));
        page.setSession(m_session);
        runAndWait(page,
                   QStringLiteral("WITH RECURSIVE n(i) AS (SELECT 1 UNION ALL SELECT i + 1 FROM n) "
                                  "SELECT i FROM n"));
        QCOMPARE(page.resultPanel()->results()->model()->rowCount(), 100);
        QVERIFY(!page.resultPanel()->results()->allRowsFetched());
        p.rowLimit = 100'000;
        Settings::instance().setPreferences(p);
    }

    void filesKeepTheirLineEnds()
    {
        const QString path = m_dir.filePath(QStringLiteral("script.sql"));
        QFile file(path);
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write("SELECT 1;\r\nSELECT 2;\r\n");
        file.close();
        EditorPage page(m_browser, QStringLiteral("Script"));
        QVERIFY(page.openFile(path));
        QVERIFY(!page.isModified());
        QVERIFY(page.title().startsWith(QLatin1String("script.sql")));
        page.editor()->append(QStringLiteral("SELECT 3;"));
        QVERIFY(page.isModified());
        QVERIFY(page.saveFile(path));
        QVERIFY(file.open(QIODevice::ReadOnly));
        QVERIFY(file.readAll().contains("SELECT 2;\r\nSELECT 3;"));
    }
};

QTEST_MAIN(TestEditorPage)
#include "tst_editorpage.moc"
