// SPDX-FileCopyrightText: 2026 Petr Vanek
// SPDX-License-Identifier: GPL-3.0-or-later

#include "Session.h"

#include "Settings.h"

#include <QFileInfo>

#include <sqlite3.h>

namespace sqliteman {

Session::Session(const QString &path, QObject *parent)
    : QObject(parent), m_path(path), m_connection(new sqlite::Connection(this)),
      m_runner(new sqlite::QueryRunner(m_connection, this))
{
    connect(m_connection, &sqlite::Connection::stateChanged, this, &Session::onStateChanged);
    connect(m_connection, &sqlite::Connection::transactionChanged, this, [this](bool open) {
        // A rollback can undo DDL, a commit makes it last: either way the
        // schema may be other than what was read during the transaction.
        if (!open)
            reloadSnapshot();
        Q_EMIT transactionChanged(open);
    });
    // Every query goes through this connection, so this sees every
    // statement that changes the schema, whoever runs it.
    connect(m_connection, &sqlite::Connection::resultReady, this, [this](const sqlite::Result &r) {
        m_schemaChanged = m_schemaChanged || r.changesSchema();
    });
    connect(m_connection, &sqlite::Connection::queryFinished, this, [this] {
        if (std::exchange(m_schemaChanged, false))
            reloadSnapshot();
    });
}

Session::~Session() = default;

QString Session::name() const
{
    if (m_path == QLatin1String(":memory:"))
        return tr("In-memory database");
    return QFileInfo(m_path).fileName();
}

sqlite::QueryRunner *Session::runner() const
{
    return m_state == State::Closed ? nullptr : m_runner;
}

void Session::open(bool create, bool readOnly)
{
    const config::Preferences &prefs = Settings::instance().preferences();
    sqlite::OpenOptions options;
    options.create = create;
    options.readOnly = readOnly;
    options.busyTimeoutMs = prefs.busyTimeoutMs;
    options.foreignKeys = prefs.foreignKeys;
    options.loadExtensions = prefs.allowExtensions;
    m_error.clear();
    m_snapshot.reset();
    m_extensions.clear();
    setState(State::Opening);
    m_connection->open(m_path, options);
}

void Session::close()
{
    m_connection->close();
    m_snapshot.reset();
    m_extensions.clear();
    setState(State::Closed);
}

void Session::onStateChanged(sqlite::Connection::State state)
{
    switch (state) {
    case sqlite::Connection::State::Ready:
        if (m_state == State::Opening) {
            setState(State::Open);
            loadStartupExtensions();
            reloadSnapshot();
        }
        break;
    case sqlite::Connection::State::Failed:
        m_error = m_connection->errorMessage();
        setState(State::Failed);
        break;
    case sqlite::Connection::State::Closed:
        if (m_state != State::Opening)
            setState(State::Closed);
        break;
    default:
        break;
    }
}

void Session::setState(State state)
{
    if (m_state == state)
        return;
    m_state = state;
    Q_EMIT stateChanged(state);
}

void Session::reloadSnapshot()
{
    if (!isOpen())
        return;
    if (m_loading) {
        // Something changed the schema while this load was on its way: what
        // comes back is out of date already, so go round again.
        m_stale = true;
        return;
    }
    m_loading = true;
    m_runner->perform(
        [](sqlite3 *db) {
            sqlite::TaskOutcome outcome;
            outcome.value = QVariant::fromValue(catalog::loadSnapshot(db));
            return outcome;
        },
        this,
        [this](const sqlite::QueryOutcome &outcome) {
            m_loading = false;
            if (outcome.ok()) {
                m_snapshot = outcome.task.value.value<catalog::SnapshotPtr>();
                Q_EMIT snapshotChanged();
            }
            if (std::exchange(m_stale, false))
                reloadSnapshot();
        });
}

void Session::loadExtension(const QString &path, QObject *context,
                            std::function<void(const QString &)> done)
{
    if (!isOpen()) {
        done(tr("The database is not open."));
        return;
    }
    m_runner->perform(
        [path](sqlite3 *db) {
            sqlite::TaskOutcome outcome;
            char *error = nullptr;
            if (sqlite3_load_extension(db, path.toUtf8().constData(), nullptr, &error)
                != SQLITE_OK) {
                outcome.error = QString::fromUtf8(error ? error : sqlite3_errmsg(db));
                sqlite3_free(error);
            }
            return outcome;
        },
        context,
        [this, path, done = std::move(done)](const sqlite::QueryOutcome &outcome) {
            if (outcome.ok()) {
                m_extensions.removeAll(path);
                m_extensions << path;
                // Its functions and modules are there now.
                reloadSnapshot();
            }
            done(outcome.error);
        });
}

void Session::loadStartupExtensions()
{
    const config::Preferences &prefs = Settings::instance().preferences();
    if (!prefs.allowExtensions)
        return;
    for (const QString &path : prefs.startupExtensions) {
        loadExtension(path, this, [this, path](const QString &error) {
            if (error.isEmpty())
                Q_EMIT message(tr("Loaded the extension %1").arg(path), false);
            else
                Q_EMIT message(tr("Could not load the extension %1: %2").arg(path, error), true);
        });
    }
}

} // namespace sqliteman
