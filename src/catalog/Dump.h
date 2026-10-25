// SPDX-FileCopyrightText: 2007-2026 Petr Vanek
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "sqlite/Connection.h"

#include <QString>

#include <atomic>

class QTextStream;
struct sqlite3;

namespace sqliteman::catalog {

struct DumpOptions
{
    QString schema = QStringLiteral("main"); // Which database: main, temp or attached.
    bool data = true; // INSERTs for the rows; without, the schema alone.
    bool transaction = true; // BEGIN and COMMIT around it, which make reading it back fast.
};

// Writes a database as SQL that makes it again, the way the sqlite3
// shell's .dump does: tables with their rows, then indexes, triggers and
// views; sqlite_sequence's counters as they are. Runs on the connection's
// thread, reading the rows straight from SQLite; stop, if given, ends it
// early. Returns the error, empty on success.
QString dumpDatabase(sqlite3 *db, const DumpOptions &options, QTextStream &out,
                     const std::atomic<bool> *stop = nullptr);

// The same as a task that writes to a file, atomically: the file is
// replaced only once all of it is written.
sqlite::Task dumpTask(const DumpOptions &options, const QString &path);

} // namespace sqliteman::catalog
