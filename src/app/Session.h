// SPDX-FileCopyrightText: 2026 Petr Vanek
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "catalog/Snapshot.h"
#include "sqlite/Connection.h"
#include "sqlite/QueryRunner.h"

#include <QObject>
#include <QStringList>

namespace sqliteman {

// One open database file: its connection, which the browser, the editors
// and the dialogs share, one query after another, and what is known of its
// schema. Sharing it is what SQLite wants: ATTACH, temporary tables and
// loaded extensions belong to a connection, and a second connection writing
// the same file would wait for this one's locks.
class Session : public QObject
{
    Q_OBJECT

public:
    enum class State { Closed, Opening, Open, Failed };
    Q_ENUM(State)

    Session(const QString &path, QObject *parent = nullptr);
    ~Session() override;

    QString path() const { return m_path; }
    // The file's name, for tabs and the browser.
    QString name() const;
    State state() const { return m_state; }
    bool isOpen() const { return m_state == State::Open; }
    QString errorMessage() const { return m_error; }
    bool isReadOnly() const { return m_connection->isReadOnly(); }
    bool inTransaction() const { return m_connection->inTransaction(); }

    // Opens the file with the options of the preferences; create makes it
    // if it is not there. Startup extensions are loaded once it is open.
    void open(bool create = false, bool readOnly = false);
    // Rolls back what is open and closes the file.
    void close();

    sqlite::Connection *connection() const { return m_connection; }
    // Runs queries; queued until the database is open. Null once closed.
    sqlite::QueryRunner *runner() const;

    // The schema of every database the connection has attached; null until
    // read. Read again after statements that change it, and on reload.
    catalog::SnapshotPtr snapshot() const { return m_snapshot; }
    void reloadSnapshot();

    // Loads an extension into the connection; done says what went wrong, if
    // anything. Needs extensions allowed in the preferences.
    void loadExtension(const QString &path, QObject *context,
                       std::function<void(const QString &error)> done);
    QStringList extensions() const { return m_extensions; }

Q_SIGNALS:
    void stateChanged(sqliteman::Session::State state);
    void snapshotChanged();
    void transactionChanged(bool inTransaction);
    // Something worth a line in the Messages of whoever shows them.
    void message(const QString &text, bool error);

private:
    void onStateChanged(sqlite::Connection::State state);
    void setState(State state);
    void loadStartupExtensions();

    QString m_path;
    sqlite::Connection *m_connection = nullptr;
    sqlite::QueryRunner *m_runner = nullptr;
    State m_state = State::Closed;
    QString m_error;
    catalog::SnapshotPtr m_snapshot;
    bool m_loading = false;
    bool m_stale = false; // Changed again while loading.
    bool m_schemaChanged = false; // By the query running now.
    QStringList m_extensions;
};

} // namespace sqliteman
