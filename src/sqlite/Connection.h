// SPDX-FileCopyrightText: 2026 Petr Vanek
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "sqlite/Result.h"
#include "sqlite/Value.h"

#include <QObject>
#include <QString>
#include <QVariant>

#include <functional>
#include <memory>
#include <vector>

struct sqlite3;

namespace sqliteman::sqlite {

struct OpenOptions
{
    bool readOnly = false;
    bool create = false; // Make the file if it is not there.
    // How long a statement waits for another program's lock before it fails
    // with SQLITE_BUSY.
    int busyTimeoutMs = 5000;
    bool foreignKeys = true; // PRAGMA foreign_keys, off in SQLite itself.
    bool loadExtensions = false; // Lets sqlite3_load_extension() work; never the SQL function.
};

struct ExecuteOptions
{
    // Rows come in results of up to this many, so that the first of a big
    // result can be shown while the rest is read; 0 sends all at the end.
    int chunkSize = 0;
    // A statement that only reads stops after this many rows, its result
    // marked truncated; 0 reads them all.
    int maxRows = 0;
    // Parameters for ?, ?NNN, :name and the like, by position. Each set runs
    // the statement once, and the SQL must then be a single statement; a
    // single Done result tells how many rows they changed in total.
    std::vector<std::vector<Value>> bindings;
};

// What a task run on the connection's thread hands back.
struct TaskOutcome
{
    QString error; // Empty on success.
    QVariant value;
};
using Task = std::function<TaskOutcome(sqlite3 *db)>;

// One SQLite database connection, worked on a thread of its own so that
// nothing the database does, waiting for a lock included, blocks the
// caller's thread. Use it only from the thread that made it. Signal
// receivers may call any method, including close(), or delete it.
class Connection : public QObject
{
    Q_OBJECT

public:
    enum class State { Closed, Opening, Ready, Busy, Failed };
    Q_ENUM(State)

    explicit Connection(QObject *parent = nullptr);
    ~Connection() override;

    // Opens a database file; ":memory:" and file: URIs work too. The file is
    // read right away, so one that is not a database fails here.
    void open(const QString &path, const OpenOptions &options = {});
    // Rolls back what is open and closes the file without waiting.
    void close();

    State state() const { return m_state; }
    QString path() const { return m_path; }
    bool isReadOnly() const { return m_readOnly; }
    // Why opening failed; empty unless state() == Failed.
    QString errorMessage() const { return m_error; }
    // A transaction is open: BEGIN ran, or a statement holds one.
    bool inTransaction() const { return m_inTransaction; }

    // Runs sql, which may hold several statements, one after another. Each
    // statement's result arrives through resultReady(), its rows in chunks,
    // then queryFinished(). A failed statement ends the query. Returns false
    // unless state() == Ready.
    bool execute(const QByteArray &sql, const ExecuteOptions &options = {});
    // Runs task with the connection's handle on its thread, for what the C
    // API does beyond SQL, like loading an extension or a backup. Its outcome
    // arrives through taskFinished(). Returns false unless state() == Ready.
    bool perform(Task task);

    // Stops the running query or task: the statement running fails with
    // SQLITE_INTERRUPT and those after it do not run. Returns false if
    // nothing is running.
    bool cancel();

    // The SQLite library this program uses.
    static QString libraryVersion();

Q_SIGNALS:
    void stateChanged(sqliteman::sqlite::Connection::State state);
    void resultReady(const sqliteman::sqlite::Result &result);
    void queryFinished();
    void taskFinished(const sqliteman::sqlite::TaskOutcome &outcome);
    void transactionChanged(bool inTransaction);

private:
    class Worker;

    void setState(State state);
    void onOpened(const QString &error, bool readOnly);
    void onFinished(bool inTransaction);

    std::shared_ptr<Worker> m_worker;
    State m_state = State::Closed;
    QString m_path;
    QString m_error;
    bool m_readOnly = false;
    bool m_inTransaction = false;
    quint64 m_generation = 0; // Ignores what a worker of an earlier open() still sends.
    quint64 m_ticket = 0; // Numbers the jobs, for cancel().
};

} // namespace sqliteman::sqlite

Q_DECLARE_METATYPE(sqliteman::sqlite::TaskOutcome)
