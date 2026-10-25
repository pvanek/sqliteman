// SPDX-FileCopyrightText: 2026 Petr Vanek
// SPDX-License-Identifier: GPL-3.0-or-later

#include "FileBrowser.h"

#include <QComboBox>
#include <QCompleter>
#include <QDir>
#include <QFileDialog>
#include <QFileSystemModel>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLineEdit>
#include <QMenu>
#include <QRegularExpression>
#include <QSettings>
#include <QToolButton>
#include <QTreeView>
#include <QVBoxLayout>

namespace sqliteman {

namespace {

const QString PathKey = QStringLiteral("files/browserPath");
const QString FilterKey = QStringLiteral("files/browserFilter");
const QString DefaultFilter = QStringLiteral("*.sql *.db *.sqlite *.sqlite3 *.db3");

} // namespace

FileBrowser::FileBrowser(QSettings &settings, QWidget *parent)
    : QWidget(parent), m_settings(settings), m_path(new QLineEdit(this)),
      m_filter(new QComboBox(this)), m_model(new QFileSystemModel(this)),
      m_view(new QTreeView(this))
{
    // The path, with folder completion, and buttons to go up and to browse.
    auto *dirs = new QFileSystemModel(this);
    dirs->setFilter(QDir::AllDirs | QDir::NoDotAndDotDot | QDir::Drives);
    dirs->setRootPath(QString());
    auto *completer = new QCompleter(dirs, this);
    completer->setCompletionMode(QCompleter::PopupCompletion);
    m_path->setCompleter(completer);
    m_path->setClearButtonEnabled(true);
    m_path->setPlaceholderText(tr("Folder"));
    connect(m_path, &QLineEdit::returnPressed, this, &FileBrowser::applyPath);
    connect(completer, qOverload<const QString &>(&QCompleter::activated), this,
            &FileBrowser::applyPath);

    auto *up = new QToolButton(this);
    up->setIcon(QIcon::fromTheme(QStringLiteral("go-up")));
    up->setText(tr("Up"));
    up->setToolTip(tr("Parent folder"));
    up->setAutoRaise(true);
    connect(up, &QToolButton::clicked, this, [this] {
        QDir dir(path());
        if (dir.cdUp())
            setPath(dir.absolutePath());
    });
    auto *browse = new QToolButton(this);
    browse->setIcon(QIcon::fromTheme(QStringLiteral("folder-open")));
    browse->setText(QStringLiteral("…"));
    browse->setToolTip(tr("Choose a folder"));
    browse->setAutoRaise(true);
    connect(browse, &QToolButton::clicked, this, [this] {
        const QString dir = QFileDialog::getExistingDirectory(this, tr("Folder"), path());
        if (!dir.isEmpty())
            setPath(dir);
    });

    auto *pathRow = new QHBoxLayout;
    pathRow->setContentsMargins(0, 0, 0, 0);
    pathRow->setSpacing(2);
    pathRow->addWidget(m_path, 1);
    pathRow->addWidget(up);
    pathRow->addWidget(browse);

    // Fixed choices; anything else can be typed, but is not added to them.
    m_filter->setEditable(true);
    m_filter->setInsertPolicy(QComboBox::NoInsert);
    m_filter->addItems({DefaultFilter, QStringLiteral("*.sql"),
                        QStringLiteral("*.db *.sqlite *.sqlite3 *.db3"), QStringLiteral("*.*")});
    m_filter->lineEdit()->setPlaceholderText(tr("Filter, like *.sql *.db"));
    m_filter->lineEdit()->setClearButtonEnabled(true);
    m_filter->setToolTip(
        tr("File name patterns, separated by spaces or semicolons. Empty or *.* shows all files."));
    connect(m_filter, &QComboBox::currentTextChanged, this, &FileBrowser::setFilter);

    // Files that do not match are hidden, not greyed out; folders always show.
    m_model->setFilter(QDir::AllDirs | QDir::Files | QDir::NoDotAndDotDot);
    m_model->setNameFilterDisables(false);
    m_model->setReadOnly(true);
    m_view->setModel(m_model);
    m_view->setHeaderHidden(true);
    for (int c = 1; c < m_model->columnCount(); ++c)
        m_view->hideColumn(c); // Size, type and date.
    m_view->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_view, &QTreeView::activated, this, [this](const QModelIndex &index) {
        if (!m_model->isDir(index))
            Q_EMIT fileActivated(m_model->filePath(index));
    });
    connect(m_view, &QTreeView::customContextMenuRequested, this, &FileBrowser::showContextMenu);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(2, 2, 2, 0);
    layout->setSpacing(2);
    layout->addLayout(pathRow);
    layout->addWidget(m_filter);
    layout->addWidget(m_view);

    setFilter(m_settings.value(FilterKey, DefaultFilter).toString());
    if (!setPath(m_settings.value(PathKey).toString()))
        setPath(QDir::homePath());
}

QString FileBrowser::path() const
{
    return m_model->rootPath();
}

bool FileBrowser::setPath(const QString &path)
{
    const QFileInfo info(QDir::fromNativeSeparators(path.trimmed()));
    if (path.trimmed().isEmpty() || !info.isDir()) {
        m_path->setStyleSheet(QStringLiteral("color: #c62828;"));
        return false;
    }
    const QString absolute = info.absoluteFilePath();
    m_path->setStyleSheet(QString());
    m_path->setText(QDir::toNativeSeparators(absolute));
    m_view->setRootIndex(m_model->setRootPath(absolute));
    m_settings.setValue(PathKey, absolute);
    return true;
}

void FileBrowser::applyPath()
{
    setPath(m_path->text());
}

QString FileBrowser::filter() const
{
    return m_filter->currentText();
}

void FileBrowser::setFilter(const QString &filter)
{
    if (m_filter->currentText() != filter)
        m_filter->setCurrentText(filter); // Comes back here through currentTextChanged.
    QStringList patterns
        = filter.split(QRegularExpression(QStringLiteral("[\\s;,]+")), Qt::SkipEmptyParts);
    // *.* means every file, like on Windows, also those without a dot.
    if (patterns.contains(QStringLiteral("*.*")))
        patterns.clear();
    m_model->setNameFilters(patterns);
    m_settings.setValue(FilterKey, filter);
}

void FileBrowser::showContextMenu(const QPoint &pos)
{
    const QModelIndex index = m_view->indexAt(pos);
    if (!index.isValid())
        return;
    QMenu menu(this);
    const QString path = m_model->filePath(index);
    if (m_model->isDir(index))
        menu.addAction(tr("Show from Here"), this, [this, path] { setPath(path); });
    else
        menu.addAction(tr("Open"), this, [this, path] { Q_EMIT fileActivated(path); });
    menu.exec(m_view->viewport()->mapToGlobal(pos));
}

} // namespace sqliteman
