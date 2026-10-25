// SPDX-FileCopyrightText: 2026 Petr Vanek
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "catalog/Snapshot.h"
#include "sqlite/QueryRunner.h"
#include "sqlite/RowStore.h"

#include <QTemporaryDir>
#include <QTest>

#include <optional>

// A database file of its own for a test, worked through the same
// Connection and QueryRunner the program uses, waited for in place.
class TestDatabase
{
public:
    explicit TestDatabase(const QString &name = QStringLiteral("test.db")) : m_runner(&m_connection)
    {
        sqliteman::sqlite::OpenOptions options;
        options.create = true;
        m_path = m_dir.filePath(name);
        m_connection.open(m_path, options);
        QTRY_VERIFY_WITH_TIMEOUT(
            m_connection.state() == sqliteman::sqlite::Connection::State::Ready, 5000);
    }

    QString path() const { return m_path; }
    QString dir() const { return m_dir.path(); }
    sqliteman::sqlite::Connection &connection() { return m_connection; }
    sqliteman::sqlite::QueryRunner &runner() { return m_runner; }

    sqliteman::sqlite::QueryOutcome run(const QByteArray &sql,
                                        const sqliteman::sqlite::ExecuteOptions &options = {})
    {
        std::optional<sqliteman::sqlite::QueryOutcome> outcome;
        m_runner.run(sql, &m_context, [&](const auto &o) { outcome = o; }, options);
        wait(outcome);
        return outcome.value_or(sqliteman::sqlite::QueryOutcome {});
    }

    // Runs sql and fails the test if it does not work.
    void exec(const QByteArray &sql)
    {
        const auto outcome = run(sql);
        if (!outcome.ok())
            QFAIL(qPrintable(QStringLiteral("%1\n%2").arg(QString::fromUtf8(sql), outcome.error)));
    }

    // The rows of the last statement in sql.
    sqliteman::sqlite::RowStore rows(const QByteArray &sql)
    {
        sqliteman::sqlite::RowStore store;
        store.append(run(sql).rows());
        return store;
    }

    QString value(const QByteArray &sql)
    {
        const sqliteman::sqlite::RowStore store = rows(sql);
        return store.rowCount() > 0 ? QString::fromUtf8(store.value(0, 0)) : QString();
    }

    sqliteman::sqlite::QueryOutcome perform(sqliteman::sqlite::Task task)
    {
        std::optional<sqliteman::sqlite::QueryOutcome> outcome;
        m_runner.perform(std::move(task), &m_context, [&](const auto &o) { outcome = o; });
        wait(outcome);
        return outcome.value_or(sqliteman::sqlite::QueryOutcome {});
    }

    sqliteman::catalog::SnapshotPtr snapshot()
    {
        const auto outcome = perform([](sqlite3 *db) {
            sqliteman::sqlite::TaskOutcome o;
            o.value = QVariant::fromValue(sqliteman::catalog::loadSnapshot(db));
            return o;
        });
        return outcome.task.value.value<sqliteman::catalog::SnapshotPtr>();
    }

private:
    template<typename T>
    static void wait(const std::optional<T> &outcome)
    {
        QTRY_VERIFY_WITH_TIMEOUT(outcome.has_value(), 10000);
    }

    QTemporaryDir m_dir;
    QString m_path;
    sqliteman::sqlite::Connection m_connection;
    sqliteman::sqlite::QueryRunner m_runner;
    QObject m_context;
};
