// SPDX-FileCopyrightText: 2007-2026 Petr Vanek
// SPDX-License-Identifier: GPL-3.0-or-later

#include "MainWindow.h"

#include "DatabaseBrowser.h"
#include "EditorPage.h"
#include "FileBrowser.h"
#include "HelpWindow.h"
#include "Icons.h"
#include "ObjectPage.h"
#include "PageTabWidget.h"
#include "PageWindow.h"
#include "PragmaPage.h"
#include "PreferencesDialog.h"
#include "QueryBuilderDialog.h"
#include "ResultModel.h"
#include "ResultPanel.h"
#include "ResultView.h"
#include "Session.h"
#include "Settings.h"
#include "SqlEditor.h"
#include "ErdView.h"
#include "TableData.h"
#include "config/RecentFiles.h"
#include "sqlite/Connection.h"

#include <QApplication>
#include <QCloseEvent>
#include <QDesktopServices>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QLabel>
#include <QMenuBar>
#include <QMessageBox>
#include <QSplitter>
#include <QStatusBar>
#include <QTabWidget>
#include <QTreeView>

namespace sqliteman {

namespace {

const QString ProjectUrl = QStringLiteral("https://github.com/pvanek/sqliteman");
const QString RecentKey = QStringLiteral("recent/databases");
const QString OpenKey = QStringLiteral("session/databases");

} // namespace

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent), m_mainSplitter(new QSplitter(Qt::Horizontal, this)),
      m_left(new QTabWidget(m_mainSplitter)), m_browser(new DatabaseBrowser(m_left)),
      m_files(new FileBrowser(Settings::instance().store(), m_left)),
      m_pages(new PageTabWidget(m_mainSplitter))
{
    QSettings &settings = Settings::instance().store();
    m_left->setDocumentMode(true);
    m_left->setTabPosition(QTabWidget::West);
    m_left->addTab(m_browser, Icons::database(true), tr("Databases"));
    m_left->addTab(m_files, QIcon::fromTheme(QStringLiteral("folder")), tr("Files"));
    m_left->setCurrentIndex(settings.value(QStringLiteral("window/leftTab"), 0).toInt());
    connect(m_left, &QTabWidget::currentChanged, this, [](int i) {
        Settings::instance().store().setValue(QStringLiteral("window/leftTab"), i);
    });
    connect(m_files, &FileBrowser::fileActivated, this,
            [this](const QString &path) { open(path); });

    m_mainSplitter->addWidget(m_left);
    m_mainSplitter->addWidget(m_pages);
    m_mainSplitter->setStretchFactor(1, 1);
    setCentralWidget(m_mainSplitter);

    m_pages->setMain(true);
    watchTabs(m_pages);
    connect(m_pages, &QTabWidget::currentChanged, this, [this] {
        if (auto *editor = qobject_cast<EditorPage *>(m_pages->currentWidget()))
            editor->editor()->setFocus();
    });
    // Save and the like follow the active window.
    connect(qApp, &QApplication::focusChanged, this, &MainWindow::updateActions);

    connect(m_browser, &DatabaseBrowser::editorRequested, this,
            [this](Session *s, const QString &sql) { newEditor(s, sql); });
    connect(m_browser, &DatabaseBrowser::objectRequested, this, &MainWindow::showObject);
    connect(m_browser, &DatabaseBrowser::diagramRequested, this,
            [this](Session *session, const QString &schema, const QString &name) {
                showObject(session, schema, name)->showDiagram();
            });
    connect(m_browser, &DatabaseBrowser::pragmasRequested, this, &MainWindow::showPragmas);
    connect(m_browser, &DatabaseBrowser::message, this, [this](const QString &text, bool error) {
        statusBar()->showMessage(text, error ? 0 : 5000);
    });
    connect(m_browser, &DatabaseBrowser::databaseOpened, this, [this](Session *s) {
        if (s->path() != QLatin1String(":memory:")) {
            config::RecentFiles(Settings::instance().store(), RecentKey)
                .add(s->path(), Settings::instance().preferences().recentCount);
            updateRecentMenu();
        }
        // An editor with nothing to work on yet takes the first database.
        for (WorkspacePage *page : pages()) {
            if (auto *editor = qobject_cast<EditorPage *>(page); editor && !editor->session())
                editor->setSession(s);
        }
        setWindowTitle(s->name());
        updateActions();
    });
    connect(m_browser, &DatabaseBrowser::sessionsChanged, this, [this] {
        // The pages of a database that went away go with it.
        const std::vector<Session *> open = m_browser->sessions();
        for (WorkspacePage *page : pages()) {
            Session *s = nullptr;
            if (auto *o = qobject_cast<ObjectPage *>(page))
                s = o->session();
            else if (auto *p = qobject_cast<PragmaPage *>(page))
                s = p->session();
            else
                continue;
            if (!s || std::ranges::find(open, s) == open.end()) {
                if (PageTabWidget *tabs = tabsOf(page))
                    tabs->takePage(page);
                page->deleteLater();
            }
        }
        if (m_pages->count() == 0)
            newEditor();
        updateActions();
    });

    setupMenus();
    auto *version = new QLabel(tr("SQLite %1").arg(sqlite::Connection::libraryVersion()), this);
    version->setToolTip(tr("The SQLite library this program uses"));
    statusBar()->addPermanentWidget(version);
    newEditor();

    resize(1280, 800);
    m_mainSplitter->setSizes({320, 960});
    restoreGeometry(settings.value(QStringLiteral("window/geometry")).toByteArray());
    m_mainSplitter->restoreState(
        settings.value(QStringLiteral("window/mainSplitter")).toByteArray());
    if (Settings::instance().preferences().reopenDatabases) {
        for (const QString &path : settings.value(OpenKey).toStringList()) {
            if (QFileInfo::exists(path))
                m_browser->openDatabase(path);
        }
    }
    updateActions();
}

MainWindow::~MainWindow()
{
    // Close the pages first: while the tab widgets delete them they would
    // report the remaining, half-destroyed ones as current.
    disconnect(qApp, nullptr, this, nullptr);
    m_browser->disconnect(this);
    for (PageWindow *window : std::as_const(m_windows)) {
        window->tabs()->disconnect(this);
        delete window;
    }
    m_pages->disconnect(this);
    while (m_pages->count() > 0)
        delete m_pages->widget(0);
}

void MainWindow::watchTabs(PageTabWidget *tabs)
{
    connect(tabs, &QTabWidget::tabCloseRequested, this, [this, tabs](int index) {
        closePage(qobject_cast<WorkspacePage *>(tabs->widget(index)));
    });
    connect(tabs, &QTabWidget::currentChanged, this, &MainWindow::updateActions);
    connect(tabs, &PageTabWidget::detachRequested, this,
            [this](WorkspacePage *page) { detachPage(page); });
    connect(tabs, &PageTabWidget::moveToMainRequested, this,
            [this](WorkspacePage *page) { movePage(page, m_pages); });
}

WorkspacePage *MainWindow::currentPage() const
{
    for (PageWindow *window : m_windows) {
        if (window->isActiveWindow())
            return window->tabs()->currentPage();
    }
    return m_pages->currentPage();
}

EditorPage *MainWindow::currentEditor() const
{
    return qobject_cast<EditorPage *>(currentPage());
}

QList<WorkspacePage *> MainWindow::pages() const
{
    QList<WorkspacePage *> out = m_pages->pages();
    for (const PageWindow *window : m_windows)
        out += window->tabs()->pages();
    return out;
}

PageTabWidget *MainWindow::tabsOf(const WorkspacePage *page) const
{
    if (m_pages->indexOf(page) >= 0)
        return m_pages;
    for (PageWindow *window : m_windows) {
        if (window->tabs()->indexOf(page) >= 0)
            return window->tabs();
    }
    return nullptr;
}

void MainWindow::addPage(WorkspacePage *page)
{
    m_pages->addPage(page);
    connect(page, &WorkspacePage::titleChanged, this, [this, page] { updateTab(page); });
}

void MainWindow::updateTab(WorkspacePage *page)
{
    if (PageTabWidget *tabs = tabsOf(page))
        tabs->updatePage(page);
    for (PageWindow *window : std::as_const(m_windows)) {
        if (window->tabs()->currentPage() == page)
            window->setWindowTitle(page->title());
    }
}

PageWindow *MainWindow::detachPage(WorkspacePage *page)
{
    PageTabWidget *from = tabsOf(page);
    if (!from)
        return nullptr;
    // A window of its own, independent of this one: its own taskbar entry,
    // free to go behind it or to another screen. This window deletes it.
    auto *window = new PageWindow;
    window->setWindowIcon(windowIcon());
    m_windows << window;
    watchTabs(window->tabs());
    connect(window->tabs(), &PageTabWidget::emptied, window, &QObject::deleteLater);
    connect(window, &QObject::destroyed, this, [this, window] {
        m_windows.removeAll(window);
        updateActions();
    });

    from->takePage(page);
    window->tabs()->addPage(page);
    window->setWindowTitle(page->title());
    const QRect main = frameGeometry();
    window->resize(QSize(main.width() * 3 / 5, main.height() * 3 / 5).expandedTo(QSize(500, 350)));
    const int step = 40 * int(m_windows.size());
    window->move(main.topLeft() + QPoint(80 + step, 80 + step));
    window->show();
    return window;
}

void MainWindow::movePage(WorkspacePage *page, PageTabWidget *to)
{
    PageTabWidget *from = tabsOf(page);
    if (!from || from == to)
        return;
    from->takePage(page);
    to->addPage(page);
    to->window()->raise();
    to->window()->activateWindow();
}

void MainWindow::showPage(WorkspacePage *page)
{
    if (PageTabWidget *tabs = tabsOf(page)) {
        tabs->setCurrentWidget(page);
        tabs->window()->raise();
        tabs->window()->activateWindow();
    }
}

bool MainWindow::closePage(WorkspacePage *page)
{
    PageTabWidget *tabs = page ? tabsOf(page) : nullptr;
    if (!tabs || !page->maybeClose())
        return false;
    tabs->takePage(page);
    delete page;
    // This window always has something to work in; page windows just go.
    if (m_pages->count() == 0)
        newEditor();
    return true;
}

bool MainWindow::isDatabaseFile(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return false;
    const QByteArray header = file.read(16);
    // An empty file is a database SQLite has not written yet, unless it is
    // named as a script.
    if (header.isEmpty())
        return !path.endsWith(QLatin1String(".sql"), Qt::CaseInsensitive);
    return header == QByteArray("SQLite format 3\0", 16);
}

void MainWindow::open(const QString &path)
{
    // As in the sqlite3 shell: a database that lives only while it is open.
    if (path == QLatin1String(":memory:") || isDatabaseFile(path))
        m_browser->openDatabase(path);
    else
        openFile(path);
}

EditorPage *MainWindow::newEditor(Session *session, const QString &sql)
{
    auto *page = new EditorPage(m_browser, tr("Script %1").arg(++m_editorCount));
    connect(page, &EditorPage::createViewRequested, this,
            [this](Session *s, const QString &select) {
                m_browser->createView(s, QStringLiteral("main"), select);
            });
    connect(page, &EditorPage::buildQueryRequested, this, &MainWindow::buildQuery);
    if (!session)
        session = m_browser->currentSession();
    if (session)
        page->setSession(session);
    if (!sql.isEmpty()) {
        page->editor()->setText(sql);
        page->editor()->setModified(false);
    }
    addPage(page);
    page->editor()->setFocus();
    return page;
}

EditorPage *MainWindow::openFile(const QString &path)
{
    // Already open: just show it.
    const QString absolute = QFileInfo(path).absoluteFilePath();
    for (WorkspacePage *page : pages()) {
        auto *open = qobject_cast<EditorPage *>(page);
        if (open && open->filePath() == absolute) {
            showPage(open);
            return open;
        }
    }
    EditorPage *editor = currentEditor();
    if (!editor || !editor->isBlank())
        editor = newEditor(editor ? editor->session() : nullptr);
    QString error;
    if (!editor->openFile(path, &error)) {
        QMessageBox::warning(this, tr("Open File"), tr("Could not open %1:\n%2").arg(path, error));
        return nullptr;
    }
    showPage(editor);
    return editor;
}

ObjectPage *MainWindow::showObject(Session *session, const QString &schema, const QString &name)
{
    for (WorkspacePage *page : pages()) {
        auto *object = qobject_cast<ObjectPage *>(page);
        if (object && object->session() == session && object->schema() == schema
            && object->name() == name) {
            showPage(object);
            object->refresh();
            return object;
        }
    }
    auto *page = new ObjectPage(session, schema, name);
    connect(page, &ObjectPage::objectRequested, this, &MainWindow::showObject);
    if (TableData *rows = page->data()) {
        connect(rows, &TableData::populateRequested, m_browser, &DatabaseBrowser::populate);
        connect(rows, &TableData::importRequested, m_browser, &DatabaseBrowser::importCsv);
    }
    addPage(page);
    return page;
}

PragmaPage *MainWindow::showPragmas(Session *session, const QString &schema)
{
    for (WorkspacePage *page : pages()) {
        auto *pragmas = qobject_cast<PragmaPage *>(page);
        if (pragmas && pragmas->session() == session && pragmas->schema() == schema) {
            showPage(pragmas);
            pragmas->refresh();
            return pragmas;
        }
    }
    auto *page = new PragmaPage(session, schema);
    addPage(page);
    return page;
}

void MainWindow::updateActions()
{
    const bool editor = currentEditor() != nullptr;
    if (m_save) {
        const auto *object = qobject_cast<ObjectPage *>(currentPage());
        m_save->setEnabled(editor || (object && object->data()));
        m_saveAs->setEnabled(editor);
    }
    const bool database = m_browser->currentSession() != nullptr;
    for (QAction *a : std::as_const(m_databaseActions))
        a->setEnabled(database);
}

void MainWindow::openWithDialog()
{
    QSettings &settings = Settings::instance().store();
    const QString dir = settings.value(QStringLiteral("files/lastDirectory")).toString();
    const QStringList paths = QFileDialog::getOpenFileNames(
        this, tr("Open"), dir,
        tr("Databases and scripts (*.db *.sqlite *.sqlite3 *.db3 *.s3db *.sql);;"
           "SQLite databases (*.db *.sqlite *.sqlite3 *.db3 *.s3db);;"
           "SQL scripts (*.sql);;All files (*)"));
    for (const QString &path : paths)
        open(path);
    if (!paths.isEmpty())
        settings.setValue(QStringLiteral("files/lastDirectory"),
                          QFileInfo(paths.first()).absolutePath());
}

void MainWindow::saveCurrent()
{
    WorkspacePage *page = currentPage();
    if (auto *object = qobject_cast<ObjectPage *>(page)) {
        if (object->data())
            object->data()->saveChanges();
        return;
    }
    EditorPage *editor = qobject_cast<EditorPage *>(page);
    if (!editor)
        return;
    // In the result grid, with edits pending, Save means those.
    const QWidget *focus = QApplication::focusWidget();
    QList<ResultView *> views = editor->resultPanel()->pinnedResults();
    views.prepend(editor->resultPanel()->results());
    for (ResultView *results : std::as_const(views)) {
        if (focus && results->isAncestorOf(focus) && results->model()->hasChanges()) {
            editor->saveChanges(results);
            return;
        }
    }
    editor->save();
}

void MainWindow::buildQuery(EditorPage *page)
{
    Session *session = page ? page->session() : m_browser->currentSession();
    if (!session)
        return;
    QueryBuilderDialog dialog(session->snapshot(), this);
    if (dialog.exec() != QDialog::Accepted)
        return;
    if (!page)
        page = newEditor(session);
    SqlEditor *editor = page->editor();
    // On a line of its own, where the cursor is, ready to run.
    editor->replaceSelectedText(QString());
    const qsizetype at = editor->cursorPosition();
    const QByteArray text = (at > 0 ? "\n" : "") + dialog.statement().toUtf8() + '\n';
    editor->replaceRange(at, at, text);
    editor->setCursorPosition(at + text.size() - 2);
    editor->setFocus();
}

void MainWindow::showPreferences()
{
    PreferencesDialog dialog(Settings::instance().preferences(), this);
    if (dialog.exec() == QDialog::Accepted)
        Settings::instance().setPreferences(dialog.preferences());
}

void MainWindow::updateRecentMenu()
{
    m_recent->clear();
    const QStringList paths = config::RecentFiles(Settings::instance().store(), RecentKey).list();
    int n = 0;
    for (const QString &path : paths) {
        if (++n > Settings::instance().preferences().recentCount)
            break;
        // &1 to &9; the tenth would collide with the first.
        const QString label = n < 10 ? QStringLiteral("&%1 %2").arg(n).arg(path) : path;
        m_recent->addAction(label, this, [this, path] { m_browser->openDatabase(path); });
    }
    if (!paths.isEmpty()) {
        m_recent->addSeparator();
        m_recent->addAction(tr("&Clear the List"), this, [this] {
            config::RecentFiles(Settings::instance().store(), RecentKey).clear();
            updateRecentMenu();
        });
    }
    m_recent->setEnabled(!paths.isEmpty());
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    for (WorkspacePage *page : pages()) {
        showPage(page);
        if (!page->maybeClose()) {
            event->ignore();
            return;
        }
    }
    QStringList open;
    for (const Session *s : m_browser->sessions())
        if (s->path() != QLatin1String(":memory:"))
            open << s->path();
    if (!m_browser->closeAll()) {
        event->ignore();
        return;
    }
    QSettings &settings = Settings::instance().store();
    settings.setValue(OpenKey, open);
    settings.setValue(QStringLiteral("window/geometry"), saveGeometry());
    settings.setValue(QStringLiteral("window/mainSplitter"), m_mainSplitter->saveState());
    for (PageWindow *window : std::as_const(m_windows)) {
        window->tabs()->disconnect(this);
        window->hide();
        window->deleteLater();
    }
    event->accept();
}

void MainWindow::setupMenus()
{
    QMenu *file = menuBar()->addMenu(tr("&File"));
    file->addAction(m_browser->newAction());
    QAction *open = file->addAction(QIcon::fromTheme(QStringLiteral("document-open")), tr("&Open…"),
                                    this, &MainWindow::openWithDialog);
    open->setShortcut(QKeySequence::Open);
    open->setToolTip(tr("Open a database or an SQL script"));
    m_recent = file->addMenu(tr("Open &Recent"));
    updateRecentMenu();
    file->addAction(m_browser->closeAction());
    file->addSeparator();
    QAction *editor = file->addAction(QIcon::fromTheme(QStringLiteral("document-new")),
                                      tr("New SQL &Editor"), this, [this] { newEditor(); });
    editor->setShortcut(QKeySequence::New);
    m_save = file->addAction(QIcon::fromTheme(QStringLiteral("document-save")), tr("&Save"), this,
                             &MainWindow::saveCurrent);
    m_save->setShortcut(QKeySequence::Save);
    m_saveAs = file->addAction(QIcon::fromTheme(QStringLiteral("document-save-as")),
                               tr("Save &As…"), this, [this] {
                                   if (EditorPage *page = currentEditor())
                                       page->saveAs();
                               });
    m_saveAs->setShortcut(QKeySequence::SaveAs);
    QAction *close = file->addAction(tr("&Close Tab"), this, [this] { closePage(currentPage()); });
    close->setShortcut(QKeySequence::Close);
    // Page windows have no menus: these work there too.
    for (QAction *action : {open, m_save, m_saveAs, close})
        action->setShortcutContext(Qt::ApplicationShortcut);
    file->addSeparator();
    file->addAction(QIcon::fromTheme(QStringLiteral("configure")), tr("&Preferences…"), this,
                    &MainWindow::showPreferences);
    file->addSeparator();
    QAction *quit = file->addAction(tr("&Quit"), this, &QWidget::close);
    quit->setShortcut(QKeySequence::Quit);

    QMenu *database = menuBar()->addMenu(tr("&Database"));
    auto add = [this, database](const QIcon &icon, const QString &text, auto slot) {
        QAction *a = database->addAction(icon, text, this, slot);
        m_databaseActions << a;
        return a;
    };
    const auto current = [this](QString *schema) { return m_browser->currentSession(schema); };
    add(Icons::object(catalog::ObjectKind::Table), tr("Create &Table…"), [this, current] {
        QString schema;
        if (Session *s = current(&schema))
            m_browser->createTable(s, schema);
    });
    add(Icons::object(catalog::ObjectKind::View), tr("Create &View…"), [this, current] {
        QString schema;
        if (Session *s = current(&schema))
            m_browser->createView(s, schema);
    });
    add(QIcon::fromTheme(QStringLiteral("view-filter")), tr("&Build Query…"),
        [this] { buildQuery(currentEditor()); });
    database->addSeparator();
    add(Icons::importData(), tr("&Import CSV…"), [this, current] {
        QString schema;
        if (Session *s = current(&schema))
            m_browser->importCsv(s, schema, QString());
    });
    add(Icons::exportData(), tr("&Dump Database…"), [this, current] {
        QString schema;
        if (Session *s = current(&schema))
            m_browser->dump(s, schema, false);
    });
    add({}, tr("Export &Schema…"), [this, current] {
        QString schema;
        if (Session *s = current(&schema))
            m_browser->dump(s, schema, true);
    });
    database->addSeparator();
    add({}, tr("&Attach Database…"), [this, current] {
        if (Session *s = current(nullptr))
            m_browser->attach(s);
    });
    add({}, tr("&Load Extension…"), [this, current] {
        if (Session *s = current(nullptr))
            m_browser->loadExtension(s);
    });
    add(Icons::system(), tr("&Pragmas"), [this, current] {
        QString schema;
        if (Session *s = current(&schema))
            showPragmas(s, schema);
    });
    add({}, tr("&Maintenance…"), [this, current] {
        QString schema;
        if (Session *s = current(&schema))
            m_browser->maintenance(s, schema);
    });
    database->addSeparator();
    database->addAction(m_browser->refreshAction());

    // Issue #13: the browser by keyboard, to go from table to table.
    QMenu *view = menuBar()->addMenu(tr("&View"));
    QAction *toBrowser = view->addAction(tr("Go to &Databases"), this, [this] {
        m_left->show();
        m_left->setCurrentWidget(m_browser);
        m_browser->view()->setFocus();
    });
    toBrowser->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_B));
    QAction *toFiles = view->addAction(tr("Go to &Files"), this, [this] {
        m_left->show();
        m_left->setCurrentWidget(m_files);
        m_files->view()->setFocus();
    });
    toFiles->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_B));
    QAction *toEditor = view->addAction(tr("Go to the &Editor"), this, [this] {
        if (EditorPage *page = currentEditor())
            page->editor()->setFocus();
    });
    toEditor->setShortcut(QKeySequence(Qt::Key_F6));
    for (QAction *a : {toBrowser, toFiles, toEditor})
        a->setShortcutContext(Qt::ApplicationShortcut);

    QMenu *help = menuBar()->addMenu(tr("&Help"));
    QAction *manual = help->addAction(QIcon::fromTheme(QStringLiteral("help-contents")),
                                      tr("&Manual"), this, &MainWindow::showHelp);
    manual->setShortcut(QKeySequence::HelpContents); // F1
    manual->setShortcutContext(Qt::ApplicationShortcut);
    help->addSeparator();
    help->addAction(tr("&About Sqliteman"), this, &MainWindow::showAbout);
    help->addAction(tr("About &Qt"), qApp, &QApplication::aboutQt);
}

void MainWindow::showHelp()
{
    // The manual as built into the help file; without one, the website,
    // which is the same manual.
    if (HelpWindow::show(helpKeyword()))
        return;
    QDesktopServices::openUrl(QUrl(QStringLiteral("https://pvanek.github.io/sqliteman/")));
}

// What the window is showing decides which page the manual opens.
QString MainWindow::helpKeyword() const
{
    const WorkspacePage *page = currentPage();
    if (qobject_cast<const EditorPage *>(page))
        return QStringLiteral("editor");
    if (const auto *object = qobject_cast<const ObjectPage *>(page)) {
        const QWidget *tab = object->tabs()->currentWidget();
        if (tab && tab == object->data())
            return QStringLiteral("data");
        if (tab && object->diagram() && tab->isAncestorOf(object->diagram()))
            return QStringLiteral("diagrams");
        return QStringLiteral("objects");
    }
    if (qobject_cast<const PragmaPage *>(page))
        return QStringLiteral("pragmas");
    return {};
}

void MainWindow::showAbout()
{
    QMessageBox::about(this, tr("About Sqliteman"),
                       tr("<h3>Sqliteman %1</h3>"
                          "<p>SQLite databases made easy.</p>"
                          "<p>SQLite %2, Qt %3</p>"
                          "<p><a href=\"%4\">%4</a></p>"
                          "<p>Copyright 2007-2026 Petr Vanek and contributors. Based on Igor "
                          "Khanin's LiteMan.</p>"
                          "<p>Licensed under the GNU General Public License v3 or later.</p>")
                           .arg(QApplication::applicationVersion(),
                                sqlite::Connection::libraryVersion(),
                                QString::fromLatin1(qVersion()), ProjectUrl));
}

} // namespace sqliteman
