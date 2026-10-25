// SPDX-FileCopyrightText: 2026 Petr Vanek
// SPDX-License-Identifier: GPL-3.0-or-later

#include "sqlite/Connection.h"

#include "sqlite/Direct.h"
#include "sqlite/Handles.h"

#include <QFileInfo>
#include <QPointer>

#include <sqlite3.h>

#include <atomic>
#include <condition_variable>
#include <deque>
#include <limits>
#include <mutex>
#include <thread>
#include <utility>

namespace sqliteman::sqlite {

namespace {

QString errorText(sqlite3 *db, int rc)
{
    const char *message = db ? sqlite3_errmsg(db) : nullptr;
    return QString::fromUtf8(message && *message ? message : sqlite3_errstr(rc));
}

// Notes what the statement being prepared does to the schema; the
// authorizer sees every action a statement takes before it runs.
int authorize(void *changesSchema, int action, const char *, const char *, const char *,
              const char *)
{
    switch (action) {
    case SQLITE_CREATE_INDEX:
    case SQLITE_CREATE_TABLE:
    case SQLITE_CREATE_TEMP_INDEX:
    case SQLITE_CREATE_TEMP_TABLE:
    case SQLITE_CREATE_TEMP_TRIGGER:
    case SQLITE_CREATE_TEMP_VIEW:
    case SQLITE_CREATE_TRIGGER:
    case SQLITE_CREATE_VIEW:
    case SQLITE_CREATE_VTABLE:
    case SQLITE_DROP_INDEX:
    case SQLITE_DROP_TABLE:
    case SQLITE_DROP_TEMP_INDEX:
    case SQLITE_DROP_TEMP_TABLE:
    case SQLITE_DROP_TEMP_TRIGGER:
    case SQLITE_DROP_TEMP_VIEW:
    case SQLITE_DROP_TRIGGER:
    case SQLITE_DROP_VIEW:
    case SQLITE_DROP_VTABLE:
    case SQLITE_ALTER_TABLE:
    case SQLITE_ATTACH:
    case SQLITE_DETACH:
        *static_cast<bool *>(changesSchema) = true;
        break;
    default:
        break;
    }
    return SQLITE_OK;
}

bool onlyCommentsLeft(const char *from, const char *to)
{
    // What sqlite3_prepare() leaves after the last statement it found: if it
    // prepares to nothing, there is nothing there.
    while (from < to && (*from == ' ' || *from == '\t' || *from == '\n' || *from == '\r'))
        ++from;
    return from == to;
}

void addCell(RowChunk &chunk, sqlite3_stmt *stmt, int c)
{
    switch (sqlite3_column_type(stmt, c)) {
    case SQLITE_INTEGER:
        chunk.add(ValueType::Integer, QByteArray::number(sqlite3_column_int64(stmt, c)));
        break;
    case SQLITE_FLOAT: {
        const Value v = Value::real(sqlite3_column_double(stmt, c));
        chunk.add(v.type(), v.data());
        break;
    }
    case SQLITE_TEXT: {
        const auto *text = reinterpret_cast<const char *>(sqlite3_column_text(stmt, c));
        chunk.add(ValueType::Text, QByteArrayView(text, sqlite3_column_bytes(stmt, c)));
        break;
    }
    case SQLITE_BLOB: {
        const auto *blob = static_cast<const char *>(sqlite3_column_blob(stmt, c));
        chunk.add(ValueType::Blob, QByteArrayView(blob, sqlite3_column_bytes(stmt, c)));
        break;
    }
    default:
        chunk.add(ValueType::Null, {});
        break;
    }
}

std::shared_ptr<const std::vector<Column>> readColumns(sqlite3_stmt *stmt)
{
    auto columns = std::make_shared<std::vector<Column>>();
    const int count = sqlite3_column_count(stmt);
    const auto text = [](const char *s) { return s ? QString::fromUtf8(s) : QString(); };
    for (int c = 0; c < count; ++c) {
        columns->push_back(
            {text(sqlite3_column_name(stmt, c)), text(sqlite3_column_decltype(stmt, c)),
             text(sqlite3_column_database_name(stmt, c)), text(sqlite3_column_table_name(stmt, c)),
             text(sqlite3_column_origin_name(stmt, c))});
    }
    return columns;
}

} // namespace

// The thread a connection's database is worked on. It is shared with the
// thread itself, which keeps it until its last job is done, so that a
// connection deleted in the middle of a query need not wait for it.
class Connection::Worker
{
public:
    using Job = std::function<void()>;

    explicit Worker(Connection *owner) : m_owner(owner)
    {
        m_thread = std::thread([this] { loop(); });
    }

    ~Worker() = default;

    // Hands a job to the thread; jobs run in the order they were given.
    void post(Job job)
    {
        {
            const std::lock_guard lock(m_mutex);
            m_jobs.push_back(std::move(job));
        }
        m_wake.notify_one();
    }

    // Runs f on the owner's thread, unless the owner is gone by then.
    template<typename F>
    void reply(quint64 generation, F f)
    {
        const std::lock_guard lock(m_mutex);
        if (!m_owner)
            return;
        QPointer<Connection> owner = m_owner;
        QMetaObject::invokeMethod(
            m_owner,
            [owner, generation, f = std::move(f)]() mutable {
                if (owner && owner->m_generation == generation)
                    f(owner.data());
            },
            Qt::QueuedConnection);
    }

    // The owner is going away: the thread closes the database and ends.
    void detach(const std::shared_ptr<Worker> &self)
    {
        {
            const std::lock_guard lock(m_mutex);
            m_owner = nullptr;
            m_jobs.clear();
            m_stopping = true;
        }
        interrupt();
        m_wake.notify_one();
        // The thread holds on to its worker until it is done.
        m_self = self;
        m_thread.detach();
    }

    void interrupt()
    {
        const std::lock_guard lock(m_dbMutex);
        if (m_db)
            sqlite3_interrupt(m_db);
    }

    void open(quint64 generation, const QString &path, const OpenOptions &options);
    void closeDatabase();
    void execute(quint64 generation, quint64 ticket, const QByteArray &sql,
                 const ExecuteOptions &options);
    void perform(quint64 generation, quint64 ticket, const Task &task);

    // Jobs numbered up to this one are cancelled, as far as they got.
    std::atomic<quint64> cancelledUpTo {0};

private:
    void loop();
    bool cancelled(quint64 ticket) const { return cancelledUpTo.load() >= ticket; }
    void send(quint64 generation, std::shared_ptr<Result::Data> data)
    {
        reply(generation,
              [r = Result(std::move(data))](Connection *c) { Q_EMIT c->resultReady(r); });
    }
    void finish(quint64 generation)
    {
        const bool inTransaction = m_db && !sqlite3_get_autocommit(m_db);
        reply(generation, [inTransaction](Connection *c) { c->onFinished(inTransaction); });
    }

    Connection *m_owner; // Guarded by m_mutex.
    std::mutex m_mutex;
    std::condition_variable m_wake;
    std::deque<Job> m_jobs;
    bool m_stopping = false;
    std::thread m_thread;
    std::shared_ptr<Worker> m_self;

    std::mutex m_dbMutex; // sqlite3_interrupt() must not meet sqlite3_close().
    sqlite3 *m_db = nullptr;
};

void Connection::Worker::loop()
{
    for (;;) {
        Job job;
        {
            std::unique_lock lock(m_mutex);
            m_wake.wait(lock, [this] { return m_stopping || !m_jobs.empty(); });
            if (m_stopping)
                break;
            job = std::move(m_jobs.front());
            m_jobs.pop_front();
        }
        job();
    }
    closeDatabase();
    // The last owner of this worker may be this thread itself.
    std::shared_ptr<Worker> self = std::move(m_self);
}

void Connection::Worker::open(quint64 generation, const QString &path, const OpenOptions &options)
{
    closeDatabase();
    int flags = SQLITE_OPEN_URI;
    flags |= options.readOnly ? SQLITE_OPEN_READONLY : SQLITE_OPEN_READWRITE;
    if (options.create && !options.readOnly)
        flags |= SQLITE_OPEN_CREATE;

    sqlite3 *db = nullptr;
    int rc = sqlite3_open_v2(path.toUtf8().constData(), &db, flags, nullptr);
    QString error;
    if (rc != SQLITE_OK) {
        error = errorText(db, rc);
        sqlite3_close_v2(db);
        db = nullptr;
    } else {
        sqlite3_extended_result_codes(db, 1);
        sqlite3_busy_timeout(db, options.busyTimeoutMs);
        sqlite3_db_config(db, SQLITE_DBCONFIG_ENABLE_LOAD_EXTENSION, options.loadExtensions ? 1 : 0,
                          nullptr);
        // Opening does not read the file: this is what finds out whether it
        // is a database at all.
        rc = sqlite3_exec(db, "SELECT count(*) FROM sqlite_schema", nullptr, nullptr, nullptr);
        if (rc == SQLITE_OK && options.foreignKeys)
            rc = sqlite3_exec(db, "PRAGMA foreign_keys = ON", nullptr, nullptr, nullptr);
        if (rc != SQLITE_OK) {
            error = errorText(db, rc);
            sqlite3_close_v2(db);
            db = nullptr;
        }
    }
    const bool readOnly = db && sqlite3_db_readonly(db, "main") == 1;
    {
        const std::lock_guard lock(m_dbMutex);
        m_db = db;
    }
    reply(generation, [error, readOnly](Connection *c) { c->onOpened(error, readOnly); });
}

void Connection::Worker::closeDatabase()
{
    const std::lock_guard lock(m_dbMutex);
    if (m_db) {
        // Every statement is finalized when it is done, so this closes at
        // once, rolling back a transaction left open.
        sqlite3_close_v2(m_db);
        m_db = nullptr;
    }
}

void Connection::Worker::execute(quint64 generation, quint64 ticket, const QByteArray &sql,
                                 const ExecuteOptions &options)
{
    const char *begin = sql.constData();
    const char *end = begin + sql.size();
    const char *tail = begin;

    const auto fail = [&](const char *at, int rc, qsizetype errorOffset, int binding = -1) {
        auto data = std::make_shared<Result::Data>();
        data->kind = Result::Kind::Error;
        data->offset = at - begin;
        data->error = errorText(m_db, rc);
        data->errorCode = sqlite3_extended_errcode(m_db);
        if (data->errorCode == SQLITE_OK)
            data->errorCode = rc;
        data->errorOffset = errorOffset;
        data->binding = binding;
        send(generation, std::move(data));
    };

    while (tail < end) {
        if (cancelled(ticket)) {
            auto data = std::make_shared<Result::Data>();
            data->kind = Result::Kind::Error;
            data->offset = tail - begin;
            data->error = QString::fromUtf8(sqlite3_errstr(SQLITE_INTERRUPT));
            data->errorCode = SQLITE_INTERRUPT;
            send(generation, std::move(data));
            break;
        }

        bool changesSchema = false;
        sqlite3_set_authorizer(m_db, &authorize, &changesSchema);
        sqlite3_stmt *raw = nullptr;
        const char *next = nullptr;
        const int prepared = sqlite3_prepare_v3(m_db, tail, int(end - tail), 0, &raw, &next);
        sqlite3_set_authorizer(m_db, nullptr, nullptr);
        StatementPtr stmt(raw);
        if (prepared != SQLITE_OK) {
            const int at = sqlite3_error_offset(m_db);
            fail(tail, prepared, at >= 0 ? (tail - begin) + at : -1);
            break;
        }
        if (!stmt) {
            // Whitespace or a comment: nothing to run.
            tail = next;
            continue;
        }
        if (!options.bindings.empty() && !onlyCommentsLeft(next, end)) {
            auto data = std::make_shared<Result::Data>();
            data->kind = Result::Kind::Error;
            data->offset = next - begin;
            data->error = QObject::tr("Only a single statement can take parameters");
            data->errorCode = SQLITE_MISUSE;
            send(generation, std::move(data));
            break;
        }

        auto columns = readColumns(stmt.get());
        const int columnCount = int(columns->size());
        auto data = std::make_shared<Result::Data>();
        data->columns = columns;
        data->rows.columns = columnCount;
        data->offset = tail - begin;
        data->length = next - tail;
        data->readOnly = sqlite3_stmt_readonly(stmt.get()) != 0;
        data->changesSchema = changesSchema;

        const qint64 changesBefore = sqlite3_total_changes64(m_db);
        int rows = 0; // In this statement, for maxRows.
        bool ok = true;
        const std::size_t runs = options.bindings.empty() ? 1 : options.bindings.size();
        for (std::size_t run = 0; run < runs && ok; ++run) {
            if (!options.bindings.empty()) {
                if (run > 0) {
                    sqlite3_reset(stmt.get());
                    sqlite3_clear_bindings(stmt.get());
                }
                const int bound = bindValues(stmt.get(), options.bindings[run]);
                if (bound != SQLITE_OK) {
                    fail(tail, bound, -1, int(run));
                    ok = false;
                    break;
                }
                if (cancelled(ticket)) {
                    fail(tail, SQLITE_INTERRUPT, -1, int(run));
                    ok = false;
                    break;
                }
            }
            for (;;) {
                const int rc = sqlite3_step(stmt.get());
                if (rc == SQLITE_DONE)
                    break;
                if (rc != SQLITE_ROW) {
                    fail(tail, rc, -1, options.bindings.empty() ? -1 : int(run));
                    ok = false;
                    break;
                }
                if (options.maxRows > 0 && rows >= options.maxRows && data->readOnly) {
                    data->truncated = true;
                    break;
                }
                for (int c = 0; c < columnCount; ++c)
                    addCell(data->rows, stmt.get(), c);
                ++data->rows.rows;
                ++rows;
                if (options.chunkSize > 0 && data->rows.rows >= options.chunkSize) {
                    auto chunk = std::make_shared<Result::Data>();
                    chunk->kind = Result::Kind::Rows;
                    chunk->columns = columns;
                    chunk->offset = data->offset;
                    chunk->length = data->length;
                    chunk->readOnly = data->readOnly;
                    chunk->rows = std::exchange(data->rows, RowChunk());
                    data->rows.columns = columnCount;
                    send(generation, std::move(chunk));
                }
            }
        }
        if (!ok)
            break;
        data->changes = int(sqlite3_total_changes64(m_db) - changesBefore);
        data->lastInsertRowid = sqlite3_last_insert_rowid(m_db);
        send(generation, std::move(data));
        tail = next;
    }
    finish(generation);
}

void Connection::Worker::perform(quint64 generation, quint64 ticket, const Task &task)
{
    TaskOutcome outcome;
    if (cancelled(ticket))
        outcome.error = QString::fromUtf8(sqlite3_errstr(SQLITE_INTERRUPT));
    else if (task)
        outcome = task(m_db);
    reply(generation, [outcome](Connection *c) { Q_EMIT c->taskFinished(outcome); });
    finish(generation);
}

Connection::Connection(QObject *parent) : QObject(parent)
{
    qRegisterMetaType<Result>();
    qRegisterMetaType<TaskOutcome>();
}

Connection::~Connection()
{
    if (m_worker)
        m_worker->detach(m_worker);
}

void Connection::open(const QString &path, const OpenOptions &options)
{
    close();
    m_path = path;
    m_error.clear();
    if (!m_worker)
        m_worker = std::make_shared<Worker>(this);
    const quint64 generation = ++m_generation;
    setState(State::Opening);
    m_worker->post(
        [w = m_worker.get(), generation, path, options] { w->open(generation, path, options); });
}

void Connection::close()
{
    if (m_state == State::Closed)
        return;
    // What the worker still sends for the database open until now is not
    // wanted any more.
    ++m_generation;
    if (m_worker) {
        m_worker->cancelledUpTo.store(std::numeric_limits<quint64>::max());
        m_worker->interrupt();
        m_worker->post([w = m_worker.get()] {
            w->closeDatabase();
            w->cancelledUpTo.store(0);
        });
    }
    m_error.clear();
    const bool wasInTransaction = m_inTransaction;
    m_inTransaction = false;
    m_readOnly = false;
    setState(State::Closed);
    if (wasInTransaction)
        Q_EMIT transactionChanged(false);
}

bool Connection::execute(const QByteArray &sql, const ExecuteOptions &options)
{
    if (m_state != State::Ready)
        return false;
    setState(State::Busy);
    const quint64 generation = m_generation;
    const quint64 ticket = ++m_ticket;
    m_worker->post([w = m_worker.get(), generation, ticket, sql, options] {
        w->execute(generation, ticket, sql, options);
    });
    return true;
}

bool Connection::perform(Task task)
{
    if (m_state != State::Ready)
        return false;
    setState(State::Busy);
    const quint64 generation = m_generation;
    const quint64 ticket = ++m_ticket;
    m_worker->post([w = m_worker.get(), generation, ticket, task = std::move(task)] {
        w->perform(generation, ticket, task);
    });
    return true;
}

bool Connection::cancel()
{
    if (m_state != State::Busy)
        return false;
    m_worker->cancelledUpTo.store(m_ticket);
    m_worker->interrupt();
    return true;
}

QString Connection::libraryVersion()
{
    return QString::fromLatin1(sqlite3_libversion());
}

void Connection::setState(State state)
{
    if (m_state == state)
        return;
    m_state = state;
    Q_EMIT stateChanged(state);
}

void Connection::onOpened(const QString &error, bool readOnly)
{
    if (!error.isEmpty()) {
        m_error = error;
        setState(State::Failed);
        return;
    }
    m_readOnly = readOnly;
    setState(State::Ready);
}

void Connection::onFinished(bool inTransaction)
{
    const bool changed = inTransaction != m_inTransaction;
    m_inTransaction = inTransaction;
    setState(State::Ready);
    // Receivers may close or delete the connection.
    const QPointer<Connection> guard(this);
    Q_EMIT queryFinished();
    if (guard && changed && m_state != State::Closed)
        Q_EMIT transactionChanged(inTransaction);
}

} // namespace sqliteman::sqlite
