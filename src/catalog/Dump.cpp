// SPDX-FileCopyrightText: 2007-2026 Petr Vanek
// SPDX-License-Identifier: GPL-3.0-or-later

#include "catalog/Dump.h"

#include "sql/Keywords.h"
#include "sqlite/Direct.h"
#include "sqlite/Handles.h"
#include "sqlite/Quote.h"

#include <QCoreApplication>
#include <QSaveFile>
#include <QSet>
#include <QTextStream>

#include <sqlite3.h>

namespace sqliteman::catalog {

namespace {

struct Entry
{
    QString type;
    QString name;
    QString sql;
};

// One row of a table as a literal list, every value as SQLite stores it.
QString valuesOf(sqlite3_stmt *stmt)
{
    QStringList values;
    const int count = sqlite3_column_count(stmt);
    for (int c = 0; c < count; ++c) {
        switch (sqlite3_column_type(stmt, c)) {
        case SQLITE_INTEGER:
            values << QString::number(sqlite3_column_int64(stmt, c));
            break;
        case SQLITE_FLOAT:
            values << sqlite::literal(sqlite::Value::real(sqlite3_column_double(stmt, c)));
            break;
        case SQLITE_TEXT:
            values << sqlite::quoteLiteral(sqlite::columnText(stmt, c));
            break;
        case SQLITE_BLOB: {
            const auto *blob = static_cast<const char *>(sqlite3_column_blob(stmt, c));
            values << sqlite::blobLiteral(QByteArray(blob, sqlite3_column_bytes(stmt, c)));
            break;
        }
        default:
            values << QStringLiteral("NULL");
            break;
        }
    }
    return values.join(QLatin1Char(','));
}

QString dumpRows(sqlite3 *db, const QString &schema, const QString &table, QTextStream &out,
                 const std::atomic<bool> *stop)
{
    const QString name = sqlite::qualifiedName(schema, table);
    const QByteArray select = (QLatin1String("SELECT * FROM ") + name).toUtf8();
    sqlite3_stmt *raw = nullptr;
    if (sqlite3_prepare_v2(db, select.constData(), -1, &raw, nullptr) != SQLITE_OK)
        return QString::fromUtf8(sqlite3_errmsg(db));
    sqlite::StatementPtr stmt(raw);
    const QString into = QLatin1String("INSERT INTO ") + sqlite::quoteIdentifier(table)
        + QLatin1String(" VALUES(");
    int rc;
    while ((rc = sqlite3_step(stmt.get())) == SQLITE_ROW) {
        if (stop && stop->load())
            return QCoreApplication::translate("Dump", "Stopped.");
        out << into << valuesOf(stmt.get()) << ");\n";
    }
    return rc == SQLITE_DONE ? QString() : QString::fromUtf8(sqlite3_errmsg(db));
}

} // namespace

QString dumpDatabase(sqlite3 *db, const DumpOptions &options, QTextStream &out,
                     const std::atomic<bool> *stop)
{
    const QString schema = options.schema.isEmpty() ? QStringLiteral("main") : options.schema;
    const QByteArray master
        = (sqlite::quoteIdentifier(schema) + QLatin1String(".sqlite_schema")).toUtf8();

    // A consistent picture of it all, even while others write.
    const bool ownTransaction = sqlite3_get_autocommit(db) != 0;
    if (ownTransaction)
        sqlite::exec(db, "BEGIN");

    std::vector<Entry> entries;
    QString error = sqlite::forEachRow(db,
                                       "SELECT type, name, sql FROM " + master
                                           + " WHERE sql IS NOT NULL ORDER BY "
                                             "CASE type WHEN 'table' THEN 0 WHEN 'index' THEN 1 "
                                             "WHEN 'trigger' THEN 2 ELSE 3 END, rowid",
                                       {}, [&](sqlite3_stmt *s) {
                                           entries.push_back({sqlite::columnText(s, 0),
                                                              sqlite::columnText(s, 1),
                                                              sqlite::columnText(s, 2)});
                                       });
    QSet<QString> shadow;
    sqlite::forEachRow(
        db, "SELECT name FROM pragma_table_list WHERE schema = ?1 AND type = 'shadow'", {schema},
        [&](sqlite3_stmt *s) { shadow.insert(sqlite::columnText(s, 0).toLower()); });

    if (error.isEmpty()) {
        out << "PRAGMA foreign_keys=OFF;\n";
        if (options.transaction)
            out << "BEGIN TRANSACTION;\n";
    }
    bool sequence = false;
    for (const Entry &e : entries) {
        if (!error.isEmpty())
            break;
        if (e.type != QLatin1String("table")) {
            out << e.sql << ";\n";
            continue;
        }
        const QString lower = e.name.toLower();
        // Kept up by SQLite itself, or by the virtual table they belong to.
        if (lower == QLatin1String("sqlite_sequence")) {
            sequence = true;
            continue;
        }
        if (lower.startsWith(QLatin1String("sqlite_stat")) || shadow.contains(lower))
            continue;
        out << e.sql << ";\n";
        if (options.data)
            error = dumpRows(db, schema, e.name, out, stop);
    }
    if (error.isEmpty() && sequence && options.data) {
        out << "DELETE FROM sqlite_sequence;\n";
        error = dumpRows(db, schema, QStringLiteral("sqlite_sequence"), out, stop);
    }
    if (error.isEmpty() && options.transaction)
        out << "COMMIT;\n";
    if (ownTransaction)
        sqlite::exec(db, "COMMIT");
    out.flush();
    return error;
}

sqlite::Task dumpTask(const DumpOptions &options, const QString &path)
{
    return [options, path](sqlite3 *db) {
        sqlite::TaskOutcome outcome;
        QSaveFile file(path);
        if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
            outcome.error = file.errorString();
            return outcome;
        }
        QTextStream out(&file);
        outcome.error = dumpDatabase(db, options, out);
        if (outcome.error.isEmpty() && !file.commit())
            outcome.error = file.errorString();
        if (!outcome.error.isEmpty())
            file.cancelWriting();
        return outcome;
    };
}

} // namespace sqliteman::catalog
