// SPDX-FileCopyrightText: 2026 Petr Vanek
// SPDX-License-Identifier: GPL-3.0-or-later

#include "sqlite/QueryRunner.h"

#include <sqlite3.h>

namespace sqliteman::sqlite {

Result QueryOutcome::rows() const
{
    for (auto it = results.rbegin(); it != results.rend(); ++it) {
        if (it->columnCount() > 0 && !it->isError())
            return *it;
    }
    return {};
}

QueryRunner::QueryRunner(Connection *connection, QObject *parent)
    : QObject(parent), m_connection(connection)
{
    connect(connection, &Connection::resultReady, this, &QueryRunner::onResult);
    connect(connection, &Connection::taskFinished, this, [this](const TaskOutcome &outcome) {
        if (m_current)
            m_outcome.task = outcome;
    });
    connect(connection, &Connection::queryFinished, this, &QueryRunner::onFinished);
    connect(connection, &Connection::stateChanged, this, &QueryRunner::onStateChanged);
}

quint64 QueryRunner::run(const QByteArray &sql, QObject *context, Callback callback,
                         const ExecuteOptions &options, ResultCallback onResult)
{
    Job job;
    job.sql = sql;
    job.options = options;
    job.context = context;
    job.callback = std::move(callback);
    job.onResult = std::move(onResult);
    return enqueue(std::move(job));
}

quint64 QueryRunner::perform(Task task, QObject *context, Callback callback)
{
    Job job;
    job.task = std::move(task);
    job.context = context;
    job.callback = std::move(callback);
    return enqueue(std::move(job));
}

quint64 QueryRunner::enqueue(Job job)
{
    job.id = ++m_nextId;
    const quint64 id = job.id;
    m_queue.push_back(std::move(job));
    if (m_connection
        && (m_connection->state() == Connection::State::Failed
            || m_connection->state() == Connection::State::Closed)) {
        // Report it later, like any other outcome.
        QMetaObject::invokeMethod(
            this,
            [this] {
                if (m_connection && m_connection->state() != Connection::State::Ready
                    && m_connection->state() != Connection::State::Busy
                    && m_connection->state() != Connection::State::Opening)
                    failAll(m_connection->errorMessage());
            },
            Qt::QueuedConnection);
        return id;
    }
    startNext();
    return id;
}

void QueryRunner::cancel(quint64 id)
{
    if (m_current && m_current->id == id) {
        if (m_connection)
            m_connection->cancel();
        return;
    }
    for (auto it = m_queue.begin(); it != m_queue.end(); ++it) {
        if (it->id != id)
            continue;
        Job job = std::move(*it);
        m_queue.erase(it);
        QueryOutcome outcome;
        outcome.error = QString::fromUtf8(sqlite3_errstr(SQLITE_INTERRUPT));
        outcome.errorCode = SQLITE_INTERRUPT;
        complete(job, outcome);
        return;
    }
}

void QueryRunner::startNext()
{
    if (m_current || m_queue.empty() || !m_connection
        || m_connection->state() != Connection::State::Ready)
        return;

    m_current = std::move(m_queue.front());
    m_queue.pop_front();
    m_outcome = {};
    const bool started = m_current->task
        ? m_connection->perform(m_current->task)
        : m_connection->execute(m_current->sql, m_current->options);
    if (!started)
        failAll(m_connection->errorMessage());
}

void QueryRunner::onResult(const Result &result)
{
    if (!m_current)
        return; // Someone else's query on the same connection.
    if (m_current->onResult && m_current->context) {
        const QPointer<QueryRunner> guard(this);
        m_current->onResult(result);
        if (!guard || !m_current)
            return;
    }
    if (result.isError()) {
        m_outcome.error = result.errorMessage();
        m_outcome.errorCode = result.errorCode();
    }
    // Chunks went to whoever streams them; without one, rows add up here.
    if (result.isChunk()) {
        if (!m_current->onResult)
            m_outcome.results.push_back(result);
        return;
    }
    m_outcome.results.push_back(result);
}

void QueryRunner::onFinished()
{
    if (!m_current)
        return;

    QueryOutcome outcome = std::move(m_outcome);
    m_outcome = {};
    if (!m_current->task.operator bool() && !m_current->onResult) {
        // Chunks and the final result of each statement into one result, so
        // a callback sees whole statements.
        std::vector<Result> merged;
        std::vector<Result> pending;
        for (const Result &r : outcome.results) {
            if (r.isChunk()) {
                pending.push_back(r);
                continue;
            }
            if (!pending.empty() && !r.isError()) {
                auto data = std::make_shared<Result::Data>();
                data->kind = r.kind();
                data->columns = r.sharedColumns();
                data->rows.columns = r.columnCount();
                pending.push_back(r);
                for (const Result &part : pending) {
                    const RowChunk &c = part.chunk();
                    for (int row = 0; row < c.rows; ++row)
                        for (int col = 0; col < c.columns; ++col)
                            data->rows.add(c.type(row, col), c.value(row, col));
                    data->rows.rows += c.rows;
                }
                data->offset = r.offset();
                data->length = r.length();
                data->changes = r.changes();
                data->lastInsertRowid = r.lastInsertRowid();
                data->readOnly = r.isReadOnly();
                data->changesSchema = r.changesSchema();
                data->truncated = r.isTruncated();
                merged.push_back(Result(std::move(data)));
            } else {
                merged.push_back(r);
            }
            pending.clear();
        }
        outcome.results = std::move(merged);
    }
    if (outcome.ok() && !outcome.task.error.isEmpty()) {
        outcome.error = outcome.task.error;
        outcome.errorCode = SQLITE_ERROR;
    }

    Job job = std::move(*m_current);
    m_current.reset();
    const QPointer<QueryRunner> guard(this);
    complete(job, outcome);
    if (guard)
        startNext();
}

void QueryRunner::onStateChanged(Connection::State state)
{
    switch (state) {
    case Connection::State::Ready:
        startNext();
        break;
    case Connection::State::Failed:
        failAll(m_connection->errorMessage());
        break;
    case Connection::State::Closed:
        failAll(tr("The database is closed"));
        break;
    default:
        break;
    }
}

void QueryRunner::failAll(const QString &message)
{
    std::deque<Job> jobs;
    if (m_current)
        jobs.push_back(std::move(*m_current));
    m_current.reset();
    m_outcome = {};
    for (Job &job : m_queue)
        jobs.push_back(std::move(job));
    m_queue.clear();

    QueryOutcome outcome;
    outcome.error = message.isEmpty() ? tr("The database is not open") : message;
    outcome.errorCode = SQLITE_MISUSE;
    for (Job &job : jobs)
        complete(job, outcome); // Contexts guard the callbacks, not the runner.
}

void QueryRunner::complete(Job &job, const QueryOutcome &outcome)
{
    if (job.context && job.callback)
        job.callback(outcome);
}

} // namespace sqliteman::sqlite
