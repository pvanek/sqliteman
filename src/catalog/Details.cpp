// SPDX-FileCopyrightText: 2026 Petr Vanek
// SPDX-License-Identifier: GPL-3.0-or-later

#include "catalog/Details.h"

#include "catalog/Pragmas.h"
#include "sql/Keywords.h"
#include "sqlite/Direct.h"
#include "sqlite/Quote.h"

#include <QCoreApplication>
#include <QFileInfo>
#include <QHash>
#include <QLocale>

#include <sqlite3.h>

namespace sqliteman::catalog {

using sqlite::columnText;
using sqlite::forEachRow;

namespace {

QString qualified(const QString &schema, const QString &name)
{
    return sql::identifier(schema.isEmpty() ? QStringLiteral("main") : schema) + QLatin1Char('.')
        + sql::identifier(name);
}

IndexDetails readIndex(sqlite3 *db, const QString &schema, const QString &name)
{
    IndexDetails index;
    index.name = name;
    forEachRow(
        db, "SELECT name, desc, coll, key FROM pragma_index_xinfo(?1, ?2) WHERE key ORDER BY seqno",
        {name, schema}, [&](sqlite3_stmt *s) {
            // An expression has no name.
            QString column = sqlite3_column_type(s, 0) == SQLITE_NULL
                ? QStringLiteral("<expression>")
                : sql::identifier(columnText(s, 0));
            const QString collation = columnText(s, 2);
            if (!collation.isEmpty() && collation != QLatin1String("BINARY"))
                column += QLatin1String(" COLLATE ") + collation;
            if (sqlite3_column_int(s, 1))
                column += QLatin1String(" DESC");
            index.columns << column;
        });
    return index;
}

} // namespace

ObjectDetails loadDetails(sqlite3 *db, const QString &schema, const QString &name)
{
    ObjectDetails details;
    const SnapshotPtr snapshot = loadSnapshot(db);
    const DatabaseInfo *database = snapshot ? snapshot->database(schema) : nullptr;
    if (database) {
        for (const SchemaObject &o : database->objects) {
            if (o.name.compare(name, Qt::CaseInsensitive) == 0) {
                details.object = o;
                break;
            }
        }
    }
    if (details.object.name.isEmpty()) {
        details.error = QStringLiteral("%1.%2 does not exist.").arg(schema, name);
        return details;
    }
    const SchemaObject &object = details.object;

    if (object.kind == ObjectKind::Index) {
        IndexDetails index = readIndex(db, schema, name);
        index.sql = object.sql;
        forEachRow(
            db, "SELECT \"unique\", origin, partial FROM pragma_index_list(?1, ?2) WHERE name = ?3",
            {object.table, schema, name}, [&](sqlite3_stmt *s) {
                index.unique = sqlite3_column_int(s, 0) != 0;
                const QString origin = columnText(s, 1);
                index.origin = origin == QLatin1String("u") ? IndexDetails::Origin::Unique
                    : origin == QLatin1String("pk")         ? IndexDetails::Origin::PrimaryKey
                                                            : IndexDetails::Origin::Created;
                index.partial = sqlite3_column_int(s, 2) != 0;
            });
        details.indexes.push_back(std::move(index));
        return details;
    }
    if (object.kind != ObjectKind::Table && object.kind != ObjectKind::View
        && object.kind != ObjectKind::VirtualTable)
        return details;

    if (object.kind == ObjectKind::Table) {
        std::vector<IndexDetails> indexes;
        forEachRow(db,
                   "SELECT name, \"unique\", origin, partial FROM pragma_index_list(?1, ?2) ORDER "
                   "BY seq DESC",
                   {name, schema}, [&](sqlite3_stmt *s) {
                       IndexDetails index;
                       index.name = columnText(s, 0);
                       index.unique = sqlite3_column_int(s, 1) != 0;
                       const QString origin = columnText(s, 2);
                       index.origin = origin == QLatin1String("u") ? IndexDetails::Origin::Unique
                           : origin == QLatin1String("pk") ? IndexDetails::Origin::PrimaryKey
                                                           : IndexDetails::Origin::Created;
                       index.partial = sqlite3_column_int(s, 3) != 0;
                       indexes.push_back(std::move(index));
                   });
        for (IndexDetails &index : indexes) {
            index.columns = readIndex(db, schema, index.name).columns;
            for (const SchemaObject &o : database->objects) {
                if (o.kind == ObjectKind::Index && o.name == index.name)
                    index.sql = o.sql;
            }
        }
        details.indexes = std::move(indexes);

        details.foreignKeys = object.foreignKeys;
    }
    for (const SchemaObject &o : database->objects) {
        if (o.kind == ObjectKind::Trigger && o.table.compare(name, Qt::CaseInsensitive) == 0)
            details.triggers.push_back(o);
    }
    return details;
}

DatabaseDetails loadDatabaseDetails(sqlite3 *db, const QString &schema)
{
    DatabaseDetails details;
    const SnapshotPtr snapshot = loadSnapshot(db);
    const DatabaseInfo *database = snapshot ? snapshot->database(schema) : nullptr;
    if (!database) {
        details.error = QStringLiteral("There is no database %1.").arg(schema);
        return details;
    }
    details.database = *database;
    const auto tr = [](const char *text) { return QCoreApplication::translate("catalog", text); };
    const QLocale locale;
    auto add = [&](const QString &name, const QString &value) {
        details.properties.emplace_back(name, value);
    };

    const QFileInfo file(database->file);
    add(tr("File"),
        database->file.isEmpty()
            ? (schema == QLatin1String("temp") ? tr("temporary") : tr("in memory"))
            : database->file);
    if (file.exists())
        add(tr("Size"), locale.formattedDataSize(file.size()));
    if (const QFileInfo wal(database->file + QLatin1String("-wal")); wal.exists())
        add(tr("Write-ahead log"), locale.formattedDataSize(wal.size()));
    add(tr("Read-only"), database->readOnly ? tr("yes") : tr("no"));

    const QString prefix = sql::identifier(schema) + QLatin1Char('.');
    QHash<QString, QString> values;
    for (const char *name :
         {"page_size", "page_count", "freelist_count", "encoding", "journal_mode", "auto_vacuum",
          "user_version", "application_id", "schema_version"}) {
        const QString pragma = QString::fromLatin1(name);
        forEachRow(db, (QStringLiteral("PRAGMA ") + prefix + pragma).toUtf8(), {},
                   [&](sqlite3_stmt *s) { values.insert(pragma, columnText(s, 0)); });
    }
    const auto shown = [&](const char *name) {
        const QString pragma = QString::fromLatin1(name);
        const PragmaInfo *info = catalog::pragma(pragma);
        return info ? displayValue(*info, values.value(pragma)) : values.value(pragma);
    };
    const qint64 pageSize = values.value(QStringLiteral("page_size")).toLongLong();
    const qint64 pages = values.value(QStringLiteral("page_count")).toLongLong();
    const qint64 unused = values.value(QStringLiteral("freelist_count")).toLongLong();
    add(tr("Page size"), locale.toString(pageSize));
    add(tr("Pages"), locale.toString(pages));
    add(tr("Free pages"),
        unused > 0 ? tr("%1, %2 to reclaim by VACUUM")
                         .arg(locale.toString(unused), locale.formattedDataSize(unused * pageSize))
                   : locale.toString(unused));
    add(tr("Encoding"), shown("encoding"));
    add(tr("Journal mode"), shown("journal_mode"));
    add(tr("Auto vacuum"), shown("auto_vacuum"));
    add(tr("User version"), shown("user_version"));
    add(tr("Application ID"), shown("application_id"));
    add(tr("Schema version"), shown("schema_version"));
    add(tr("SQLite"), QString::fromLatin1(sqlite3_libversion()));

    int counts[4] = {};
    for (const SchemaObject &o : database->objects) {
        if (o.system)
            continue;
        switch (o.kind) {
        case ObjectKind::Table:
        case ObjectKind::VirtualTable:
            ++counts[0];
            break;
        case ObjectKind::View:
            ++counts[1];
            break;
        case ObjectKind::Index:
            ++counts[2];
            break;
        case ObjectKind::Trigger:
            ++counts[3];
            break;
        default:
            break;
        }
    }
    add(tr("Tables"), locale.toString(counts[0]));
    add(tr("Views"), locale.toString(counts[1]));
    add(tr("Indexes"), locale.toString(counts[2]));
    add(tr("Triggers"), locale.toString(counts[3]));
    return details;
}

QString dropStatement(ObjectKind kind, const QString &schema, const QString &name)
{
    QString what;
    switch (kind) {
    case ObjectKind::Table:
    case ObjectKind::VirtualTable:
        what = QStringLiteral("TABLE");
        break;
    case ObjectKind::View:
        what = QStringLiteral("VIEW");
        break;
    case ObjectKind::Index:
        what = QStringLiteral("INDEX");
        break;
    case ObjectKind::Trigger:
        what = QStringLiteral("TRIGGER");
        break;
    case ObjectKind::Database:
        return QStringLiteral("DETACH DATABASE %1").arg(sql::identifier(name));
    case ObjectKind::Column:
        return {};
    }
    return QStringLiteral("DROP %1 %2").arg(what, qualified(schema, name));
}

QString createIndexStatement(const QString &schema, const QString &name, const QString &table,
                             const QStringList &columns, bool unique, bool ifNotExists,
                             const QString &where)
{
    QString sql = QStringLiteral("CREATE ");
    if (unique)
        sql += QLatin1String("UNIQUE ");
    sql += QLatin1String("INDEX ");
    if (ifNotExists)
        sql += QLatin1String("IF NOT EXISTS ");
    // The index goes where its table is; the table is named without the schema.
    sql += qualified(schema, name) + QLatin1String(" ON ") + sql::identifier(table)
        + QLatin1String(" (") + columns.join(QLatin1String(", ")) + QLatin1Char(')');
    if (!where.trimmed().isEmpty())
        sql += QLatin1String(" WHERE ") + where.trimmed();
    return sql;
}

QString createViewStatement(const QString &schema, const QString &name, const QString &select)
{
    QString body = select.trimmed();
    while (body.endsWith(QLatin1Char(';')))
        body.chop(1);
    return QStringLiteral("CREATE VIEW %1 AS\n%2").arg(qualified(schema, name), body);
}

QString triggerTemplate(const QString &schema, const QString &table, bool onView)
{
    const QString when = onView ? QStringLiteral("INSTEAD OF") : QStringLiteral("AFTER");
    return QStringLiteral("CREATE TRIGGER %1\n"
                          "    %2 INSERT ON %3\n"
                          "    FOR EACH ROW\n"
                          "    -- WHEN NEW.column IS NOT NULL\n"
                          "BEGIN\n"
                          "    SELECT 1; -- NEW.column, OLD.column\n"
                          "END;")
        .arg(qualified(schema, table + QLatin1String("_trigger")), when, sql::identifier(table));
}

} // namespace sqliteman::catalog
