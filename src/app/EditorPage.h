// SPDX-FileCopyrightText: 2026 Petr Vanek
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "catalog/Export.h"
#include "catalog/Editing.h"
#include "sqlite/Result.h"

#include "ResultView.h"
// Complete, not declared: moc registers the signals' Session * argument,
// and GCC warns when that sees Session incomplete before its definition.
#include "Session.h"
#include "WorkspacePage.h"

#include <QElapsedTimer>
#include <QHash>
#include <QPointer>
#include <QSaveFile>
#include <QTextStream>

#include <deque>
#include <memory>
#include <optional>

class QAction;
class QComboBox;
class QLabel;

namespace sqliteman {

class DatabaseBrowser;
class ResultPanel;
class SearchBar;
class SqlEditor;

// An SQL editor on one of the open databases. Its results go to a
// ResultPanel below the editor, on the same page. The statements run on the
// database's own connection, which the browser and other editors share: a
// transaction begun here is the database's.
class EditorPage : public WorkspacePage
{
    Q_OBJECT

public:
    EditorPage(DatabaseBrowser *browser, const QString &name, QWidget *parent = nullptr);
    ~EditorPage() override;

    SqlEditor *editor() const { return m_editor; }
    SearchBar *searchBar() const { return m_searchBar; }
    ResultPanel *resultPanel() const { return m_panel; }
    Session *session() const { return m_session; }
    // The file name (or the tab's name), * when modified, and the database.
    QString title() const override;
    QIcon icon() const override;

    // Files. Text is read and written as UTF-8, with the file's line endings.
    QString filePath() const { return m_filePath; }
    bool isModified() const;
    // Whether nothing was typed or opened yet, so opening a file can reuse the tab.
    bool isBlank() const;
    bool openFile(const QString &path, QString *error = nullptr);
    bool saveFile(const QString &path, QString *error = nullptr);
    // Save to the file, or ask where if there is none. False if cancelled or failed.
    bool save();
    bool saveAs();
    // Before closing: asks to save changes. False to keep the tab open.
    bool maybeSave();
    bool maybeClose() override;

    // Points the editor at an open database; null for none.
    void setSession(Session *session);

    // The selected statements, or the one under the cursor.
    void run();
    // Every statement of the script, from the top.
    void runScript();
    void explain();
    void cancel();
    void begin();
    void commit();
    void rollback();
    // Writes the result's edits back to its table; confirm shows the SQL first.
    void saveChanges(ResultView *view, bool confirm = true);
    void saveChanges(bool confirm = true);
    bool isRunning() const { return m_current.has_value(); }
    // The toolbar's transaction indicator: grey (disabled) outside one.
    QLabel *transactionIndicator() const { return m_transaction; }

Q_SIGNALS:
    void runningChanged(bool running);
    // Create View on the statement: the browser's dialog does it.
    void createViewRequested(sqliteman::Session *session, const QString &select);
    // Build Query: the dialog puts its SELECT into this editor.
    void buildQueryRequested(sqliteman::EditorPage *page);

private:
    struct Job
    {
        enum class Kind {
            Statement,
            Explain,
            Silent,
            Export, // Runs the shown query again, writing every row to a file.
        };

        // An export in progress: rows go to the file as they arrive, so a
        // result far too big for the grid still fits through.
        struct Sink
        {
            catalog::ExportOptions options;
            QString path;
            std::unique_ptr<QSaveFile> file;
            std::unique_ptr<QTextStream> stream;
            std::unique_ptr<catalog::RowWriter> writer; // Writes to stream.
            int rows = 0;
        };
        Kind kind = Kind::Statement;
        QByteArray sql;
        // Where a statement's rows go: the view under Results unless set, a
        // pinned one when run again there.
        QPointer<ResultView> view;
        qsizetype offset = -1; // In the document, to mark errors; -1 for none.
        int prefixBytes = 0; // Added before the user's text, like EXPLAIN QUERY PLAN.
        bool always = false; // Runs even after an error, like ROLLBACK.
        bool onFailure = false; // Runs only after an error.
        QString message; // Logged when it succeeds.
        std::shared_ptr<Sink> sink; // Export jobs only.
    };

    void setUpResults(ResultView *view);
    void updateSessions();
    void updateActions();
    void updateStatus();
    void exportAll(ResultView *view, const catalog::ExportOptions &options, const QString &path);
    void start(std::deque<Job> jobs);
    void startNext();
    void onResult(const sqlite::Result &result);
    void onFinished(const sqlite::QueryOutcome &outcome);
    void finishAll();
    void markError(const Job &job, const sqlite::Result &result);
    static QString preview(const QByteArray &sql);
    static std::deque<Job> statementJobs(const std::vector<struct ScriptPiece> &pieces);

    DatabaseBrowser *m_browser = nullptr;
    QString m_name;
    QString m_filePath;
    SqlEditor *m_editor = nullptr;
    SearchBar *m_searchBar = nullptr;
    ResultPanel *m_panel = nullptr;
    QComboBox *m_target = nullptr;
    QLabel *m_status = nullptr;
    QLabel *m_transaction = nullptr;
    QAction *m_run = nullptr;
    QAction *m_runScript = nullptr;
    QAction *m_explain = nullptr;
    QAction *m_cancel = nullptr;
    QAction *m_begin = nullptr;
    QAction *m_commit = nullptr;
    QAction *m_rollback = nullptr;
    QAction *m_createView = nullptr;
    QAction *m_buildQuery = nullptr;

    QPointer<Session> m_session;
    std::deque<Job> m_jobs;
    std::optional<Job> m_current;
    quint64 m_currentId = 0; // In the runner, to cancel it.
    QElapsedTimer m_timer;
    bool m_failed = false; // A job failed: skip the rest but the "always" and "onFailure" ones.
    bool m_rowsShown = false; // The current job's rows went to the result view.
    qint64 m_rows = 0;
    bool m_truncated = false; // Stopped at the row limit.
    int m_changes = 0;
    // The statement whose rows each view shows, to run again after saving.
    QHash<const ResultView *, Job> m_shownQueries;
};

} // namespace sqliteman
