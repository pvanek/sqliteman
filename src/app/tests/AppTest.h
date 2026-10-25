// SPDX-FileCopyrightText: 2026 Petr Vanek
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "DatabaseBrowser.h"
#include "Session.h"
#include "sqlite/QueryRunner.h"

#include <QFile>
#include <QTemporaryDir>
#include <QTest>

#include <optional>

// What the GUI tests share: databases made from SQL in a temporary
// directory, and opened the way the program opens them.
namespace apptest {

// Makes a database file from sql and returns its path, empty on failure.
inline QString makeDatabase(const QTemporaryDir &dir, const QString &name, const QByteArray &sql)
{
    const QString path = dir.filePath(name);
    QFile::remove(path);
    sqliteman::sqlite::Connection connection;
    sqliteman::sqlite::QueryRunner runner(&connection);
    sqliteman::sqlite::OpenOptions options;
    options.create = true;
    connection.open(path, options);
    QObject context;
    std::optional<sqliteman::sqlite::QueryOutcome> outcome;
    runner.run(sql, &context, [&](const sqliteman::sqlite::QueryOutcome &o) { outcome = o; });
    if (!QTest::qWaitFor([&] { return outcome.has_value(); }, 10000) || !outcome->ok()) {
        qWarning("%s", outcome ? qPrintable(outcome->error) : "timed out");
        return {};
    }
    return path;
}

// Opens it in the browser and waits until its schema is read.
inline sqliteman::Session *open(sqliteman::DatabaseBrowser &browser, const QString &path)
{
    sqliteman::Session *session = browser.openDatabase(path);
    if (!session
        || !QTest::qWaitFor([session] { return session->isOpen() && session->snapshot(); }, 10000))
        return nullptr;
    return session;
}

// Runs sql on a session and waits for it.
inline sqliteman::sqlite::QueryOutcome run(sqliteman::Session *session, const QByteArray &sql)
{
    QObject context;
    std::optional<sqliteman::sqlite::QueryOutcome> outcome;
    session->runner()->run(sql, &context,
                           [&](const sqliteman::sqlite::QueryOutcome &o) { outcome = o; });
    (void)QTest::qWaitFor([&] { return outcome.has_value(); }, 10000);
    return outcome.value_or(sqliteman::sqlite::QueryOutcome {});
}

inline QString value(sqliteman::Session *session, const QByteArray &sql)
{
    const auto outcome = run(session, sql);
    const auto rows = outcome.rows();
    return rows.rowCount() > 0 ? rows.valueAt(0, 0).toString() : QString();
}

// Waits until the session has read the schema anew, after something
// changed it.
inline bool waitForSchema(sqliteman::Session *session,
                          const std::function<bool(const sqliteman::catalog::Snapshot &)> &holds)
{
    return QTest::qWaitFor([&] { return session->snapshot() && holds(*session->snapshot()); },
                           10000);
}

} // namespace apptest
