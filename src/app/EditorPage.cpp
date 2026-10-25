// SPDX-FileCopyrightText: 2026 Petr Vanek
// SPDX-License-Identifier: GPL-3.0-or-later

#include "EditorPage.h"

#include "DatabaseBrowser.h"
#include "Icons.h"
#include "PlanView.h"
#include "ResultModel.h"
#include "ResultPanel.h"
#include "ResultView.h"
#include "SaveChangesDialog.h"
#include "SearchBar.h"
#include "Session.h"
#include "Settings.h"
#include "SqlEditor.h"
#include "catalog/Plan.h"
#include "sql/Lexer.h"

#include <QAction>
#include <QComboBox>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QLabel>
#include <QMessageBox>
#include <QRegularExpression>
#include <QSplitter>
#include <QStyle>
#include <QToolBar>
#include <QToolButton>
#include <QVBoxLayout>

#include <sqlite3.h>

namespace sqliteman {

namespace {

// Where in a statement SQLite's message points, when it says no position
// itself: "no such table: x" names what it did not find.
qsizetype namedInMessage(const QByteArray &sql, const QString &message)
{
    static const QRegularExpression named(
        QStringLiteral("^no such (?:table|column|function|collation sequence|index|view): "
                       "(?:\\S+\\.)?(\\S+)"));
    const QRegularExpressionMatch m = named.match(message);
    if (!m.hasMatch())
        return -1;
    const QString name = m.captured(1);
    for (const sql::Token &t : sql::tokenize(sql)) {
        if (t.kind != sql::TokenKind::Identifier && t.kind != sql::TokenKind::QuotedIdentifier)
            continue;
        const QString token
            = QString::fromUtf8(sql::unquote(QByteArrayView(sql).sliced(t.offset, t.length)));
        if (token.compare(name, Qt::CaseInsensitive) == 0)
            return t.offset;
    }
    return -1;
}

} // namespace

EditorPage::EditorPage(DatabaseBrowser *browser, const QString &name, QWidget *parent)
    : WorkspacePage(parent), m_browser(browser), m_name(name), m_editor(new SqlEditor(this)),
      m_panel(new ResultPanel(this)), m_target(new QComboBox(this)), m_status(new QLabel(this)),
      m_transaction(new QLabel(this))
{
    auto action
        = [this](const QIcon &icon, const QString &text, const QKeySequence &key, auto slot) {
              auto *a = new QAction(icon, text, this);
              a->setShortcut(key);
              a->setShortcutContext(Qt::WidgetWithChildrenShortcut);
              const QString plain = QString(text).remove(QLatin1Char('&'));
              a->setToolTip(key.isEmpty() ? plain
                                          : QStringLiteral("%1 (%2)").arg(
                                                plain, key.toString(QKeySequence::NativeText)));
              connect(a, &QAction::triggered, this, slot);
              addAction(a);
              return a;
          };
    m_run = action(Icons::run(), tr("&Run Statement"), QKeySequence(Qt::CTRL | Qt::Key_Return),
                   &EditorPage::run);
    m_runScript
        = action(Icons::runScript(), tr("Run &Script"),
                 QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_Return), &EditorPage::runScript);
    m_explain = action(Icons::explain(), tr("&Explain Query Plan"),
                       QKeySequence(Qt::CTRL | Qt::Key_E), &EditorPage::explain);
    m_cancel = action(Icons::stop(), tr("&Cancel"), QKeySequence(Qt::CTRL | Qt::Key_Period),
                      &EditorPage::cancel);
    m_begin = action(Icons::transaction(), tr("&Begin"), {}, &EditorPage::begin);
    m_begin->setToolTip(tr("Begin a transaction"));
    m_commit = action(Icons::commit(), tr("C&ommit"), {}, &EditorPage::commit);
    m_commit->setToolTip(tr("Commit the transaction"));
    m_rollback = action(Icons::rollback(), tr("Ro&llback"), {}, &EditorPage::rollback);
    m_rollback->setToolTip(tr("Roll the transaction back"));
    m_createView
        = action(Icons::object(catalog::ObjectKind::View), tr("Create &View…"), {}, [this] {
              const auto pieces = m_editor->piecesToRun();
              if (m_session && pieces.size() == 1)
                  Q_EMIT createViewRequested(m_session, QString::fromUtf8(pieces.front().sql));
          });
    m_createView->setToolTip(tr("Make a view of the statement under the cursor"));
    m_buildQuery = action(QIcon::fromTheme(QStringLiteral("view-filter")), tr("&Build Query…"), {},
                          [this] { Q_EMIT buildQueryRequested(this); });
    m_buildQuery->setToolTip(tr("Put a SELECT together from lists"));
    // Ctrl+Enter on the keypad too.
    auto *enter = new QAction(this);
    enter->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_Enter));
    enter->setShortcutContext(Qt::WidgetWithChildrenShortcut);
    connect(enter, &QAction::triggered, this, &EditorPage::run);
    addAction(enter);

    m_target->setSizeAdjustPolicy(QComboBox::AdjustToContents);
    m_target->setToolTip(tr("The database this editor runs statements on"));
    connect(m_target, &QComboBox::activated, this, [this](int index) {
        setSession(m_target->itemData(index).value<QPointer<Session>>());
    });

    auto *toolbar = new QToolBar(this);
    toolbar->setIconSize(QSize(16, 16));
    toolbar->addWidget(m_target);
    toolbar->addSeparator();
    toolbar->addAction(m_run);
    toolbar->addAction(m_runScript);
    toolbar->addAction(m_explain);
    toolbar->addAction(m_cancel);
    toolbar->addSeparator();
    toolbar->addWidget(m_transaction);
    toolbar->addWidget(m_status);
    for (QAction *a : {m_begin, m_commit, m_rollback}) {
        toolbar->addAction(a);
        if (auto *button = qobject_cast<QToolButton *>(toolbar->widgetForAction(a)))
            button->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    }
    toolbar->addSeparator();
    toolbar->addAction(m_buildQuery);
    toolbar->addAction(m_createView);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    layout->addWidget(toolbar);
    // The editor with its search bar below it, as in Kate. Their keys work
    // only there: in the results, Ctrl+R runs the query again.
    auto *editorArea = new QWidget(this);
    m_searchBar = new SearchBar(m_editor, editorArea);
    auto *editorLayout = new QVBoxLayout(editorArea);
    editorLayout->setContentsMargins(0, 0, 0, 0);
    editorLayout->setSpacing(0);
    editorLayout->addWidget(m_editor);
    editorLayout->addWidget(m_searchBar);
    editorArea->addActions(m_searchBar->editorActions());

    auto *splitter = new QSplitter(Qt::Vertical, this);
    splitter->setChildrenCollapsible(false);
    splitter->addWidget(editorArea);
    splitter->addWidget(m_panel);
    splitter->setStretchFactor(0, 3);
    splitter->setStretchFactor(1, 2);
    layout->addWidget(splitter);

    connect(m_browser, &DatabaseBrowser::sessionsChanged, this, &EditorPage::updateSessions);
    connect(m_editor, &QsciScintilla::modificationChanged, this, &EditorPage::titleChanged);
    setUpResults(m_panel->results());
    connect(m_panel, &ResultPanel::resultsReplaced, this, &EditorPage::setUpResults);
    connect(m_panel, &ResultPanel::pinnedClosing, this, [this](ResultView *view) {
        // Nothing else is to wait for rows nobody can see any more.
        m_shownQueries.remove(view);
        if (m_current && m_current->view == view) {
            m_current->view = nullptr;
            if (m_current->kind == Job::Kind::Statement)
                cancel();
        }
        std::erase_if(m_jobs, [view](const Job &job) { return job.view == view; });
    });
    auto *separator = new QAction(this);
    separator->setSeparator(true);
    m_editor->setContextActions({m_run, m_runScript, m_explain, m_cancel, separator,
                                 m_searchBar->findAction(), m_searchBar->replaceAction()});
    m_editor->setSnapshotProvider(
        [this] { return m_session ? m_session->snapshot() : catalog::SnapshotPtr(); });

    updateSessions();
    updateActions();
    updateStatus();
}

EditorPage::~EditorPage()
{
    // Nothing is to wait for what this page started.
    if (m_session && m_session->runner() && m_currentId)
        m_session->runner()->cancel(m_currentId);
}

QString EditorPage::title() const
{
    QString name = m_filePath.isEmpty() ? m_name : QFileInfo(m_filePath).fileName();
    if (isModified())
        name += QLatin1Char('*');
    if (!m_session)
        return name;
    return name + QStringLiteral(" · ") + m_session->name();
}

QIcon EditorPage::icon() const
{
    return QIcon::fromTheme(QStringLiteral("text-x-sql"),
                            QIcon::fromTheme(QStringLiteral("text-plain")));
}

bool EditorPage::isModified() const
{
    return m_editor->isModified();
}

bool EditorPage::isBlank() const
{
    return m_filePath.isEmpty() && !isModified() && m_editor->length() == 0;
}

bool EditorPage::openFile(const QString &path, QString *error)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        if (error)
            *error = file.errorString();
        return false;
    }
    const QByteArray bytes = file.readAll();
    // Keep the file's line endings, and type new lines the same way.
    m_editor->setEolMode(bytes.contains("\r\n") ? QsciScintilla::EolWindows
                                                : QsciScintilla::EolUnix);
    m_editor->setText(QString::fromUtf8(bytes));
    m_editor->setModified(false);
    m_editor->setCursorPosition(0);
    m_filePath = QFileInfo(path).absoluteFilePath();
    setToolTip(m_filePath);
    Q_EMIT titleChanged();
    return true;
}

bool EditorPage::saveFile(const QString &path, QString *error)
{
    // Written to a temporary file first, so a failed save leaves the old one.
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly) || file.write(m_editor->utf8Text()) < 0
        || !file.commit()) {
        if (error)
            *error = file.errorString();
        return false;
    }
    m_editor->setModified(false);
    m_filePath = QFileInfo(path).absoluteFilePath();
    setToolTip(m_filePath);
    Q_EMIT titleChanged();
    return true;
}

bool EditorPage::save()
{
    if (m_filePath.isEmpty())
        return saveAs();
    QString error;
    if (saveFile(m_filePath, &error))
        return true;
    QMessageBox::warning(this, tr("Save"), tr("Could not save %1:\n%2").arg(m_filePath, error));
    return false;
}

bool EditorPage::saveAs()
{
    QSettings &settings = Settings::instance().store();
    const QString dir = m_filePath.isEmpty()
        ? settings.value(QStringLiteral("files/lastDirectory")).toString()
        : m_filePath;
    const QString path = QFileDialog::getSaveFileName(this, tr("Save SQL Script"), dir,
                                                      tr("SQL scripts (*.sql);;All files (*)"));
    if (path.isEmpty())
        return false;
    settings.setValue(QStringLiteral("files/lastDirectory"), QFileInfo(path).absolutePath());
    QString error;
    if (saveFile(path, &error))
        return true;
    QMessageBox::warning(this, tr("Save"), tr("Could not save %1:\n%2").arg(path, error));
    return false;
}

bool EditorPage::maybeSave()
{
    if (!isModified())
        return true;
    const QString name = m_filePath.isEmpty() ? m_name : QFileInfo(m_filePath).fileName();
    const auto answer = QMessageBox::question(
        this, tr("Unsaved Changes"), tr("Save the changes to %1?").arg(name),
        QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel, QMessageBox::Save);
    if (answer == QMessageBox::Save)
        return save();
    return answer == QMessageBox::Discard;
}

bool EditorPage::maybeClose()
{
    if (!maybeSave())
        return false;
    if (isRunning()) {
        // What runs is the database's, not the page's: it stops, the
        // transaction it is in stays.
        cancel();
        finishAll();
    }
    return true;
}

void EditorPage::setSession(Session *session)
{
    if (session == m_session) {
        updateSessions();
        return;
    }
    if (isRunning())
        cancel();
    m_jobs.clear();
    m_current.reset();
    if (m_session)
        m_session->disconnect(this);
    m_session = session;
    if (m_session) {
        connect(m_session, &Session::stateChanged, this, [this] {
            updateActions();
            updateStatus();
        });
        connect(m_session, &Session::transactionChanged, this, [this] {
            updateActions();
            updateStatus();
        });
    }
    updateSessions();
    updateActions();
    updateStatus();
    Q_EMIT titleChanged();
}

void EditorPage::updateSessions()
{
    // A database closed in the browser takes the editor off it.
    const std::vector<Session *> sessions = m_browser->sessions();
    if (m_session && std::ranges::find(sessions, m_session.data()) == sessions.end()) {
        setSession(nullptr);
        return;
    }
    QSignalBlocker block(m_target);
    m_target->clear();
    m_target->addItem(tr("(no database)"), QVariant::fromValue(QPointer<Session>()));
    for (Session *s : sessions) {
        m_target->addItem(Icons::database(s->isOpen()), s->name(),
                          QVariant::fromValue(QPointer<Session>(s)));
        m_target->setItemData(m_target->count() - 1, s->path(), Qt::ToolTipRole);
        if (m_session == s)
            m_target->setCurrentIndex(m_target->count() - 1);
    }
    updateActions();
}

void EditorPage::setUpResults(ResultView *view)
{
    connect(view, &ResultView::saveRequested, this, [this, view] { saveChanges(view); });
    connect(view, &ResultView::exportAllRequested, this,
            [this, view](const catalog::ExportOptions &options, const QString &path) {
                exportAll(view, options, path);
            });
}

void EditorPage::updateActions()
{
    const bool ready = m_session && m_session->isOpen() && !isRunning();
    m_run->setEnabled(ready);
    m_runScript->setEnabled(ready);
    m_explain->setEnabled(ready);
    m_cancel->setEnabled(isRunning());
    const bool open = m_session && m_session->inTransaction();
    m_begin->setEnabled(ready && !open);
    m_commit->setEnabled(ready && open);
    m_rollback->setEnabled(ready && open);
    m_createView->setEnabled(ready);
    m_buildQuery->setEnabled(m_session && m_session->isOpen());
    m_panel->setPinEnabled(!isRunning());
}

void EditorPage::updateStatus()
{
    QString text;
    if (!m_session)
        text = tr("No database");
    else if (m_session->state() == Session::State::Opening)
        text = tr("Opening…");
    else if (m_session->state() != Session::State::Open)
        text = tr("Closed");
    else if (isRunning())
        text = tr("Running…");
    m_status->setText(text);

    const bool open = m_session && m_session->inTransaction();
    const int size
        = m_transaction->style()->pixelMetric(QStyle::PM_SmallIconSize, nullptr, m_transaction);
    m_transaction->setPixmap(
        Icons::transaction().pixmap(QSize(size, size), m_transaction->devicePixelRatioF()));
    // Enabled only in a transaction: disabled, the label greys it out. Its
    // tooltip shows either way.
    m_transaction->setEnabled(open);
    m_transaction->setToolTip(open ? tr("In a transaction: commit or roll it back")
                                   : tr("Not in a transaction: each statement is committed"));
}

// Running.

std::deque<EditorPage::Job> EditorPage::statementJobs(const std::vector<ScriptPiece> &pieces)
{
    std::deque<Job> jobs;
    for (const ScriptPiece &piece : pieces) {
        if (piece.dotCommand)
            continue; // The sqlite3 shell's, not SQL; logged by the caller.
        Job job;
        job.sql = piece.sql;
        job.offset = piece.offset;
        jobs.push_back(std::move(job));
    }
    return jobs;
}

void EditorPage::run()
{
    if (!m_run->isEnabled())
        return;
    const auto pieces = m_editor->piecesToRun();
    for (const ScriptPiece &piece : pieces) {
        if (piece.dotCommand)
            m_panel->log(tr("Skipped a command of the sqlite3 shell: %1").arg(preview(piece.sql)));
    }
    std::deque<Job> jobs = statementJobs(pieces);
    if (jobs.empty()) {
        m_panel->log(tr("Nothing to run here: put the cursor in a statement or select some."));
        return;
    }
    m_panel->showResults();
    start(std::move(jobs));
}

void EditorPage::runScript()
{
    if (!m_runScript->isEnabled())
        return;
    const auto pieces = m_editor->allPieces();
    for (const ScriptPiece &piece : pieces) {
        if (piece.dotCommand)
            m_panel->log(tr("Skipped a command of the sqlite3 shell: %1").arg(preview(piece.sql)));
    }
    std::deque<Job> jobs = statementJobs(pieces);
    if (jobs.empty()) {
        m_panel->log(tr("The script has no statements."));
        return;
    }
    m_panel->log(tr("Running %n statement(s)…", nullptr, int(jobs.size())));
    m_panel->showMessages();
    start(std::move(jobs));
}

void EditorPage::explain()
{
    if (!m_explain->isEnabled())
        return;
    const auto pieces = m_editor->piecesToRun();
    if (pieces.size() != 1 || pieces.front().dotCommand) {
        m_panel->plan()->showMessage(
            tr("Put the cursor in one SQL statement, or select one, to explain it."), true);
        m_panel->showPlan();
        return;
    }
    Job job;
    job.kind = Job::Kind::Explain;
    job.sql = catalog::explainStatement(pieces.front().sql);
    job.prefixBytes = int(job.sql.size() - pieces.front().sql.size());
    job.offset = pieces.front().offset;
    m_panel->plan()->showMessage(tr("Explaining…"));
    m_panel->showPlan();
    start({job});
}

void EditorPage::begin()
{
    if (!m_begin->isEnabled())
        return;
    Job job;
    job.kind = Job::Kind::Silent;
    job.sql = "BEGIN";
    job.message = tr("Transaction started.");
    start({job});
}

void EditorPage::commit()
{
    if (!m_commit->isEnabled())
        return;
    Job job;
    job.kind = Job::Kind::Silent;
    job.sql = "COMMIT";
    job.message = tr("Committed.");
    start({job});
}

void EditorPage::rollback()
{
    if (!m_rollback->isEnabled())
        return;
    Job job;
    job.kind = Job::Kind::Silent;
    job.sql = "ROLLBACK";
    job.message = tr("Rolled back.");
    start({job});
}

void EditorPage::cancel()
{
    if (isRunning() && m_session && m_session->runner()) {
        m_session->runner()->cancel(m_currentId);
        m_panel->log(tr("Cancelling…"));
    }
}

void EditorPage::start(std::deque<Job> jobs)
{
    m_editor->clearErrors();
    m_jobs = std::move(jobs);
    m_failed = false;
    startNext();
    Q_EMIT runningChanged(isRunning());
}

void EditorPage::startNext()
{
    while (!m_jobs.empty()) {
        const Job &next = m_jobs.front();
        const bool runs = next.onFailure ? m_failed : (!m_failed || next.always);
        if (runs)
            break;
        m_jobs.pop_front();
    }
    sqlite::QueryRunner *runner = m_session ? m_session->runner() : nullptr;
    if (m_jobs.empty() || !runner) {
        finishAll();
        return;
    }

    m_current = std::move(m_jobs.front());
    m_jobs.pop_front();
    // Decided now rather than when queued: a view pinned meanwhile keeps
    // its rows.
    if (m_current->kind == Job::Kind::Statement && !m_current->view)
        m_current->view = m_panel->results();
    m_rowsShown = false;
    m_rows = 0;
    m_truncated = false;
    m_changes = 0;
    m_timer.start();

    sqlite::ExecuteOptions options;
    if (m_current->kind == Job::Kind::Statement || m_current->kind == Job::Kind::Export)
        options.chunkSize = 1000;
    if (m_current->kind == Job::Kind::Statement)
        options.maxRows = Settings::instance().preferences().rowLimit;
    m_currentId = runner->run(
        m_current->sql, this, [this](const sqlite::QueryOutcome &outcome) { onFinished(outcome); },
        options, [this](const sqlite::Result &result) { onResult(result); });
    updateActions();
    updateStatus();
}

void EditorPage::onResult(const sqlite::Result &result)
{
    if (!m_current)
        return;
    const Job &job = *m_current;

    if (result.isError()) {
        m_failed = true;
        const QString message = result.errorMessage();
        m_panel->log(message, true);
        markError(job, result);
        if (job.kind == Job::Kind::Explain) {
            m_panel->plan()->showMessage(message, true);
        } else if (job.kind == Job::Kind::Statement && !m_rowsShown && job.view) {
            job.view->showError(message);
            m_panel->setCurrentWidget(job.view);
        }
        return;
    }

    switch (job.kind) {
    case Job::Kind::Explain: {
        sqlite::RowStore rows;
        rows.append(result);
        if (result.isChunk() || rows.rowCount() == 0)
            break;
        if (auto plan = catalog::parsePlan(rows)) {
            catalog::nameTables(*plan, job.sql.mid(job.prefixBytes));
            m_panel->plan()->setPlan(*plan, preview(job.sql.mid(job.prefixBytes)));
        }
        break;
    }
    case Job::Kind::Statement:
        if (result.columnCount() > 0 && job.view) {
            if (!m_rowsShown) {
                m_rowsShown = true;
                // Run Again runs this statement again, as it was sent, into
                // the same view. The editor text may have changed since: no
                // error marks then.
                Job again = job;
                again.offset = -1;
                job.view->setRerun([this, again] {
                    if (m_run->isEnabled())
                        start(std::deque<Job> {again});
                });
                job.view->begin(preview(job.sql));
            }
            job.view->append(result);
            m_rows += result.rowCount();
        }
        if (!result.isChunk()) {
            m_changes = result.changes();
            m_truncated = result.isTruncated();
        }
        break;
    case Job::Kind::Export:
        if (job.sink) {
            // One chunk at a time, into the one document the writer keeps.
            sqlite::RowStore chunk;
            chunk.append(result);
            job.sink->writer->write(chunk);
            job.sink->rows += result.rowCount();
        }
        break;
    case Job::Kind::Silent:
        m_changes = result.changes();
        break;
    }
}

void EditorPage::onFinished(const sqlite::QueryOutcome &outcome)
{
    Q_UNUSED(outcome);
    m_currentId = 0;
    if (!m_current)
        return;
    const Job job = std::move(*m_current);
    m_current.reset();
    const qint64 ms = m_timer.elapsed();

    if (job.kind == Job::Kind::Statement) {
        QString line = preview(job.sql);
        if (m_truncated)
            line += tr(": stopped after %n row(s)", nullptr, int(m_rows));
        else if (m_rowsShown)
            line += tr(": %n row(s)", nullptr, int(m_rows));
        else if (m_changes > 0)
            line += tr(": %n row(s) changed", nullptr, m_changes);
        line += tr(" (%1 ms)").arg(ms);
        if (!m_failed)
            m_panel->log(line);
        // No view if its tab was closed meanwhile.
        if (ResultView *view = job.view) {
            if (m_rowsShown) {
                view->finish(m_truncated
                                 ? tr("First %n row(s), %1 ms", nullptr, int(m_rows)).arg(ms)
                                 : tr("%n row(s), %1 ms", nullptr, int(m_rows)).arg(ms));
            } else if (!m_failed) {
                // What is shown now is not a query's rows: nothing to run again.
                view->setRerun({});
                view->showMessage(
                    m_changes > 0 ? tr("%n row(s) changed in %1 ms.", nullptr, m_changes).arg(ms)
                                  : tr("Done in %1 ms.").arg(ms));
            }
            // The grid holds every row unless the row limit stopped the
            // query, which decides what an export of "all rows" has to do.
            view->setAllRowsFetched(!m_truncated);
            // Rows from one table can be edited: which, and how a row is found.
            if (m_rowsShown && !m_failed) {
                m_shownQueries.insert(view, job);
                ResultModel *model = view->model();
                if (m_session && m_session->snapshot()) {
                    model->setEditTarget(
                        catalog::editTarget(model->rows(), *m_session->snapshot()));
                } else {
                    catalog::EditTarget none;
                    none.reason = tr("The schema is still being read.");
                    model->setEditTarget(none);
                }
            }
        }
    } else if (job.kind == Job::Kind::Explain && !m_failed) {
        m_panel->log(
            tr("Explained: %1 (%2 ms)").arg(preview(job.sql.mid(job.prefixBytes))).arg(ms));
    } else if (job.kind == Job::Kind::Export && job.sink) {
        Job::Sink &sink = *job.sink;
        sink.writer->finish();
        sink.stream->flush();
        if (m_failed) {
            sink.file->cancelWriting();
            m_panel->log(tr("Export of %1 was not written.").arg(sink.path), true);
        } else if (!sink.file->commit()) {
            m_panel->log(tr("Could not write %1: %2").arg(sink.path, sink.file->errorString()),
                         true);
        } else {
            m_panel->log(
                tr("Exported %1 rows to %2 (%3 ms).").arg(sink.rows).arg(sink.path).arg(ms));
        }
    }
    if (!job.message.isEmpty() && !m_failed)
        m_panel->log(job.message);
    startNext();
}

void EditorPage::exportAll(ResultView *view, const catalog::ExportOptions &options,
                           const QString &path)
{
    const auto shown = m_shownQueries.constFind(view);
    if (shown == m_shownQueries.cend()) {
        m_panel->log(tr("There is no query to run again for the export."), true);
        return;
    }
    if (isRunning()) {
        m_panel->log(tr("Wait for the running statement before exporting."), true);
        return;
    }
    auto sink = std::make_shared<Job::Sink>();
    sink->options = options;
    sink->options.maxRows = 0; // Every row, whatever the grid's limit was.
    sink->options.rows.clear();
    sink->path = path;
    sink->file = std::make_unique<QSaveFile>(path);
    if (!sink->file->open(QIODevice::WriteOnly | QIODevice::Text)) {
        m_panel->log(tr("Could not write %1: %2").arg(path, sink->file->errorString()), true);
        return;
    }
    sink->stream = std::make_unique<QTextStream>(sink->file.get());
    sink->writer = std::make_unique<catalog::RowWriter>(sink->options, *sink->stream);

    Job job = *shown;
    job.kind = Job::Kind::Export;
    job.message.clear();
    job.sink = std::move(sink);
    m_panel->log(tr("Exporting every row of %1 to %2…").arg(preview(job.sql), path));
    m_panel->showMessages();
    start(std::deque<Job> {job});
}

void EditorPage::saveChanges(bool confirm)
{
    saveChanges(m_panel->results(), confirm);
}

void EditorPage::saveChanges(ResultView *view, bool confirm)
{
    ResultModel *model = view->model();
    if (!model->hasChanges() || !model->isEditable() || isRunning() || !m_session
        || !m_session->isOpen())
        return;
    const catalog::EditTarget &target = model->editTarget();
    const std::vector<catalog::DmlStatement> statements
        = catalog::dmlStatements(target, model->changes());
    const bool inTransaction = m_session->inTransaction();
    if (confirm) {
        QStringList previews;
        for (const catalog::DmlStatement &s : statements)
            previews << s.preview();
        SaveChangesDialog dialog(target.schema + QLatin1Char('.') + target.table, previews,
                                 inTransaction, this);
        if (dialog.exec() != QDialog::Accepted)
            return;
    }
    // All of them or none, then the rows as they are now.
    m_session->runner()->perform(
        [statements](sqlite3 *db) { return catalog::applyEdits(db, statements); }, this,
        [this, view, inTransaction, count = int(statements.size())](const sqlite::QueryOutcome &o) {
            if (!o.ok()) {
                m_panel->log(tr("Nothing was saved: %1").arg(o.error), true);
                m_panel->showMessages();
                return;
            }
            m_panel->log(inTransaction
                             ? tr("Saved %n change(s) in the open transaction.", nullptr, count)
                             : tr("Saved %n change(s).", nullptr, count));
            if (const auto shown = m_shownQueries.constFind(view);
                shown != m_shownQueries.cend() && !isRunning()) {
                Job again = *shown;
                again.offset = -1;
                again.view = view;
                start({again});
            } else {
                view->model()->discardChanges();
            }
        });
}

void EditorPage::finishAll()
{
    m_jobs.clear();
    m_current.reset();
    m_currentId = 0;
    updateActions();
    updateStatus();
    Q_EMIT runningChanged(false);
}

void EditorPage::markError(const Job &job, const sqlite::Result &result)
{
    if (job.offset < 0)
        return;
    qsizetype position = result.errorOffset();
    if (position < 0)
        position = namedInMessage(job.sql, result.errorMessage());
    position -= job.prefixBytes;
    if (position >= 0)
        m_editor->markError(job.offset + position, result.errorMessage());
}

QString EditorPage::preview(const QByteArray &sql)
{
    QString text = QString::fromUtf8(sql).simplified();
    if (text.size() > 60)
        text = text.left(60) + QChar(0x2026);
    return text;
}

} // namespace sqliteman
