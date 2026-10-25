// SPDX-FileCopyrightText: 2026 Petr Vanek
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "sqlite/Connection.h"
#include "sqlite/Result.h"

#include <QPointer>

#include <deque>
#include <functional>
#include <optional>
#include <vector>

namespace sqliteman::sqlite {

struct QueryOutcome
{
    // Every statement's Done result, and the error that ended the query, if
    // one did. Chunks of rows are left out when they went to onResult.
    std::vector<Result> results;
    // The error that ended the query. Empty on success.
    QString error;
    int errorCode = 0;
    TaskOutcome task; // For perform().

    bool ok() const { return error.isEmpty(); }
    // The last result that has columns: what a single SELECT returned.
    Result rows() const;
};

// Runs queries on one connection one after another, for everything that
// works with the same database: the browser, editors, dialogs. Each caller
// gets a callback when its query is done. Queries wait while the database
// is still opening.
class QueryRunner : public QObject
{
    Q_OBJECT

public:
    using Callback = std::function<void(const QueryOutcome &)>;
    // Each result as it arrives, chunks of rows included.
    using ResultCallback = std::function<void(const Result &)>;

    explicit QueryRunner(Connection *connection, QObject *parent = nullptr);

    Connection *connection() const { return m_connection; }

    // Queues sql. callback runs once it is done, or failed because the
    // database was closed, unless context has been deleted by then.
    // Callbacks may queue more queries or delete the runner. Returns an id
    // for cancel().
    quint64 run(const QByteArray &sql, QObject *context, Callback callback,
                const ExecuteOptions &options = {}, ResultCallback onResult = {});
    quint64 perform(Task task, QObject *context, Callback callback);

    // Stops a query: a queued one does not run, a running one is
    // interrupted. Its callback still runs, with the error.
    void cancel(quint64 id);
    // The query running, or 0.
    quint64 current() const { return m_current ? m_current->id : 0; }

    // Queued and running queries.
    int pending() const { return int(m_queue.size()) + (m_current ? 1 : 0); }

private:
    struct Job
    {
        quint64 id = 0;
        QByteArray sql;
        ExecuteOptions options;
        Task task;
        QPointer<QObject> context;
        Callback callback;
        ResultCallback onResult;
    };

    quint64 enqueue(Job job);
    void startNext();
    void onResult(const Result &result);
    void onFinished();
    void onStateChanged(Connection::State state);
    void failAll(const QString &message);
    static void complete(Job &job, const QueryOutcome &outcome);

    QPointer<Connection> m_connection;
    std::deque<Job> m_queue;
    std::optional<Job> m_current;
    QueryOutcome m_outcome;
    quint64 m_nextId = 0;
};

} // namespace sqliteman::sqlite
