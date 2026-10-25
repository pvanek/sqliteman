// SPDX-FileCopyrightText: 2007-2026 Petr Vanek
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "catalog/Snapshot.h"

#include <QMainWindow>

class QAction;
class QLabel;
class QMenu;
class QSplitter;
class QTabWidget;

namespace sqliteman {

class DatabaseBrowser;
class EditorPage;
class FileBrowser;
class ObjectPage;
class PageTabWidget;
class PageWindow;
class PragmaPage;
class Session;
class WorkspacePage;

// The open databases and files on the left; the work area on the right,
// with a tab per page: SQL editors with their results, a table's rows, an
// object's details, a database's pragmas.
class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

    DatabaseBrowser *browser() const { return m_browser; }
    FileBrowser *files() const { return m_files; }

    // The current page of the active window: this one or a page window.
    WorkspacePage *currentPage() const;
    EditorPage *currentEditor() const;
    QList<WorkspacePage *> pages() const;
    QList<PageWindow *> pageWindows() const { return m_windows; }
    PageTabWidget *tabsOf(const WorkspacePage *page) const;
    PageWindow *detachPage(WorkspacePage *page);
    void movePage(WorkspacePage *page, PageTabWidget *to);
    PageTabWidget *mainTabs() const { return m_pages; }
    void showPage(WorkspacePage *page);
    void addPage(WorkspacePage *page);

    // A database file, a script, or whatever the file turns out to be: an
    // SQLite file opens as a database, anything else in an editor.
    void open(const QString &path);
    static bool isDatabaseFile(const QString &path);
    // A new editor on a database; without one, on the database selected in
    // the browser, if any.
    EditorPage *newEditor(Session *session = nullptr, const QString &sql = {});
    // Opens a script in the current editor if it is blank, else in a new one.
    EditorPage *openFile(const QString &path);
    // Everything about an object, a table's or view's rows included; with an
    // empty name, about the database schema. The same object reuses its page.
    ObjectPage *showObject(Session *session, const QString &schema, const QString &name);
    PragmaPage *showPragmas(Session *session, const QString &schema);
    bool closePage(WorkspacePage *page);

protected:
    void closeEvent(QCloseEvent *event) override;

private:
    void setupMenus();
    void watchTabs(PageTabWidget *tabs);
    void updateTab(WorkspacePage *page);
    void updateActions();
    void updateRecentMenu();
    void openWithDialog();
    void saveCurrent();
    void buildQuery(EditorPage *page);
    void showPreferences();
    void showHelp();
    QString helpKeyword() const;
    void showAbout();
    // Closes the pages of a database that is being closed.
    void closePagesOf(Session *session);

    QSplitter *m_mainSplitter = nullptr;
    QTabWidget *m_left = nullptr; // Databases and files.
    DatabaseBrowser *m_browser = nullptr;
    FileBrowser *m_files = nullptr;
    PageTabWidget *m_pages = nullptr; // The work area.
    QList<PageWindow *> m_windows; // Pages dragged out of it.
    QMenu *m_recent = nullptr;
    QAction *m_save = nullptr;
    QAction *m_saveAs = nullptr;
    QList<QAction *> m_databaseActions; // Need an open database.
    int m_editorCount = 0;
};

} // namespace sqliteman
