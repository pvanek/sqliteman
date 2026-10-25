// SPDX-FileCopyrightText: 2026 Petr Vanek
// SPDX-License-Identifier: GPL-3.0-or-later

#include "HelpWindow.h"

#include <QApplication>
#include <QCoreApplication>
#include <QDesktopServices>
#include <QDir>
#include <QFileInfo>
#include <QHelpContentWidget>
#include <QHelpEngine>
#include <QHelpIndexWidget>
#include <QHelpLink>
#include <QHelpSearchEngine>
#include <QHelpSearchQueryWidget>
#include <QHelpSearchResultWidget>
#include <QLineEdit>
#include <QPointer>
#include <QSplitter>
#include <QStandardPaths>
#include <QTabWidget>
#include <QTextBrowser>
#include <QToolBar>
#include <QVBoxLayout>

namespace sqliteman {

// The pages live inside the .qch; QTextBrowser knows nothing about
// qthelp:// URLs, so it is handed the bytes.
class HelpBrowser : public QTextBrowser
{
public:
    HelpBrowser(QHelpEngine *engine, QWidget *parent) : QTextBrowser(parent), m_engine(engine)
    {
        setOpenExternalLinks(true);
    }

    QVariant loadResource(int type, const QUrl &url) override
    {
        if (url.scheme() == QLatin1String("qthelp"))
            return m_engine->fileData(url);
        return QTextBrowser::loadResource(type, url);
    }

private:
    QHelpEngine *m_engine = nullptr;
};

namespace {

QPointer<HelpWindow> theWindow;

} // namespace

QString HelpWindow::helpFilePath()
{
    // Beside the program when running from the build directory, in the data
    // directory once installed, in Resources inside a macOS bundle.
    const QString name = QStringLiteral("sqliteman.qch");
    QStringList places {QCoreApplication::applicationDirPath() + QLatin1Char('/') + name};
    for (const QString &data : QStandardPaths::standardLocations(QStandardPaths::AppDataLocation))
        places << data + QLatin1Char('/') + name;
    places << QCoreApplication::applicationDirPath() + QStringLiteral("/../share/sqliteman/")
            + name;
    places << QCoreApplication::applicationDirPath() + QStringLiteral("/../Resources/") + name;
    for (const QString &path : std::as_const(places)) {
        if (QFileInfo::exists(path))
            return QDir::cleanPath(path);
    }
    return {};
}

bool HelpWindow::show(const QString &keyword)
{
    const QString qch = helpFilePath();
    if (qch.isEmpty())
        return false;

    if (!theWindow) {
        // The engine needs somewhere of its own to keep what it knows about
        // the registered documentation and its search index.
        const QString data = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
        QDir().mkpath(data);
        auto *window = new HelpWindow(data + QStringLiteral("/help.qhc"));
        if (!window->registerDocumentation(qch)) {
            delete window;
            return false;
        }
        theWindow = window;
    }

    theWindow->openKeyword(keyword);
    theWindow->QWidget::show();
    theWindow->raise();
    theWindow->activateWindow();
    return true;
}

HelpWindow::HelpWindow(const QString &collectionFile)
    : QWidget(nullptr, Qt::Window), m_engine(new QHelpEngine(collectionFile, this)),
      m_navigation(new QTabWidget(this)), m_search(new QLineEdit(this))
{
    setAttribute(Qt::WA_DeleteOnClose);
    setWindowTitle(tr("%1 Manual").arg(QCoreApplication::applicationName()));
    resize(1000, 700);

    m_browser = new HelpBrowser(m_engine, this);
    m_browser->setOpenLinks(false); // Links are followed through the engine.
    connect(m_browser, &QTextBrowser::anchorClicked, this, &HelpWindow::openUrl);

    m_navigation->setTabPosition(QTabWidget::West);
    m_navigation->setDocumentMode(true);
    m_navigation->addTab(m_engine->contentWidget(), tr("Contents"));
    // The tree is built as the engine reads the file; show the pages, not
    // just the manual's own name.
    connect(m_engine->contentModel(), &QHelpContentModel::contentsCreated, this,
            [this] { m_engine->contentWidget()->expandToDepth(1); });
    m_navigation->addTab(m_engine->indexWidget(), tr("Index"));
    connect(m_engine->contentWidget(), &QHelpContentWidget::linkActivated, this,
            &HelpWindow::openUrl);
    connect(m_engine->indexWidget(), &QHelpIndexWidget::documentActivated, this,
            [this](const QHelpLink &document, const QString &) { openUrl(document.url); });

    // Search: the engine indexes the help file itself, so it works offline.
    auto *searchPage = new QWidget(m_navigation);
    auto *searchLayout = new QVBoxLayout(searchPage);
    searchLayout->setContentsMargins(4, 4, 4, 4);
    m_search->setPlaceholderText(tr("Search the manual"));
    searchLayout->addWidget(m_search);
    QHelpSearchEngine *searchEngine = m_engine->searchEngine();
    searchLayout->addWidget(searchEngine->resultWidget());
    connect(m_search, &QLineEdit::returnPressed, this,
            [this, searchEngine] { searchEngine->search(m_search->text()); });
    connect(searchEngine->resultWidget(), &QHelpSearchResultWidget::requestShowLink, this,
            &HelpWindow::openUrl);
    m_navigation->addTab(searchPage, tr("Search"));

    auto *toolbar = new QToolBar(this);
    toolbar->setIconSize(QSize(16, 16));
    QAction *back = toolbar->addAction(QIcon::fromTheme(QStringLiteral("go-previous")), tr("Back"));
    QAction *forward
        = toolbar->addAction(QIcon::fromTheme(QStringLiteral("go-next")), tr("Forward"));
    QAction *home = toolbar->addAction(QIcon::fromTheme(QStringLiteral("go-home")), tr("Contents"));
    back->setShortcut(QKeySequence::Back);
    forward->setShortcut(QKeySequence::Forward);
    connect(back, &QAction::triggered, m_browser, &QTextBrowser::backward);
    connect(forward, &QAction::triggered, m_browser, &QTextBrowser::forward);
    connect(home, &QAction::triggered, this, [this] { openKeyword(QString()); });
    connect(m_browser, &QTextBrowser::backwardAvailable, back, &QAction::setEnabled);
    connect(m_browser, &QTextBrowser::forwardAvailable, forward, &QAction::setEnabled);
    back->setEnabled(false);
    forward->setEnabled(false);

    auto *splitter = new QSplitter(Qt::Horizontal, this);
    splitter->addWidget(m_navigation);
    splitter->addWidget(m_browser);
    splitter->setStretchFactor(1, 1);
    splitter->setSizes({280, 720});

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    layout->addWidget(toolbar);
    layout->addWidget(splitter, 1);
}

HelpWindow::~HelpWindow() = default;

bool HelpWindow::registerDocumentation(const QString &qchFile)
{
    // A help engine is read-only until told otherwise, and a read-only one
    // refuses to register anything, quietly.
    m_engine->setReadOnly(false);
    if (!m_engine->setupData())
        return false;
    // Registered documentation is remembered in the collection file, so a
    // rebuilt manual has to replace what is there.
    const QString namespaceName = QHelpEngineCore::namespaceName(qchFile);
    if (namespaceName.isEmpty())
        return false;
    const QString registered = m_engine->documentationFileName(namespaceName);
    if (registered != qchFile
        || QFileInfo(registered).lastModified() != QFileInfo(qchFile).lastModified()) {
        m_engine->unregisterDocumentation(namespaceName);
        if (!m_engine->registerDocumentation(qchFile))
            return false;
    }
    m_engine->searchEngine()->reindexDocumentation();
    return true;
}

void HelpWindow::openKeyword(const QString &keyword)
{
    const QList<QHelpLink> documents = keyword.isEmpty()
        ? QList<QHelpLink>()
        : m_engine->documentsForIdentifier(keyword) + m_engine->documentsForKeyword(keyword);
    if (!documents.isEmpty()) {
        openUrl(documents.constFirst().url);
        return;
    }
    // Not an index entry: a keyword is also simply the name of a page.
    if (!keyword.isEmpty() && openPage(keyword))
        return;
    openPage(QStringLiteral("index")); // No such keyword: the front page.
}

// Opens <name>.html of the manual, wherever in it that page lives.
bool HelpWindow::openPage(const QString &name)
{
    const QString ending = QLatin1Char('/') + name + QStringLiteral(".html");
    for (const QString &documentation : m_engine->registeredDocumentations()) {
        const QList<QUrl> files = m_engine->files(documentation, QStringList());
        for (const QUrl &file : files) {
            if (file.path().endsWith(ending)) {
                openUrl(file);
                return true;
            }
        }
    }
    return false;
}

void HelpWindow::openUrl(const QUrl &url)
{
    if (url.scheme() != QLatin1String("qthelp")) {
        QDesktopServices::openUrl(url);
        return;
    }
    m_browser->setSource(url);
}

} // namespace sqliteman
