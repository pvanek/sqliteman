// SPDX-FileCopyrightText: 2026 Petr Vanek
// SPDX-License-Identifier: GPL-3.0-or-later

#include "catalog/Snapshot.h"

#include "sql/Lexer.h"
#include "sqlite/Direct.h"
#include "sqlite/Quote.h"

#include <QCoreApplication>
#include <QHash>

#include <sqlite3.h>

#include <algorithm>
#include <map>

namespace sqliteman::catalog {

using sqlite::columnText;
using sqlite::forEachRow;

namespace {

int kindOrder(ObjectKind kind)
{
    switch (kind) {
    case ObjectKind::Table:
        return 0;
    case ObjectKind::VirtualTable:
        return 1;
    case ObjectKind::View:
        return 2;
    case ObjectKind::Index:
        return 3;
    case ObjectKind::Trigger:
        return 4;
    default:
        return 5;
    }
}

// Whether the CREATE TABLE says AUTOINCREMENT, as a keyword rather than in
// a name or a string.
bool hasAutoincrement(const QString &sql)
{
    const QByteArray utf8 = sql.toUtf8();
    for (const sql::Token &t : sql::tokenize(utf8)) {
        if (t.kind == sql::TokenKind::Keyword
            && QByteArrayView(utf8)
                    .sliced(t.offset, t.length)
                    .compare("AUTOINCREMENT", Qt::CaseInsensitive)
                == 0)
            return true;
    }
    return false;
}

} // namespace

QString kindName(ObjectKind kind)
{
    switch (kind) {
    case ObjectKind::Database:
        return QCoreApplication::translate("catalog", "database");
    case ObjectKind::Table:
        return QCoreApplication::translate("catalog", "table");
    case ObjectKind::VirtualTable:
        return QCoreApplication::translate("catalog", "virtual table");
    case ObjectKind::View:
        return QCoreApplication::translate("catalog", "view");
    case ObjectKind::Index:
        return QCoreApplication::translate("catalog", "index");
    case ObjectKind::Trigger:
        return QCoreApplication::translate("catalog", "trigger");
    case ObjectKind::Column:
        return QCoreApplication::translate("catalog", "column");
    }
    return {};
}

std::vector<QString> SchemaObject::primaryKey() const
{
    std::vector<const ColumnInfo *> key;
    for (const ColumnInfo &c : columns) {
        if (c.primaryKey > 0)
            key.push_back(&c);
    }
    std::sort(key.begin(), key.end(), [](const ColumnInfo *a, const ColumnInfo *b) {
        return a->primaryKey < b->primaryKey;
    });
    std::vector<QString> names;
    for (const ColumnInfo *c : key)
        names.push_back(c->name);
    return names;
}

const DatabaseInfo *Snapshot::database(const QString &name) const
{
    for (const DatabaseInfo &d : databases) {
        if (d.name.compare(name, Qt::CaseInsensitive) == 0)
            return &d;
    }
    return nullptr;
}

const SchemaObject *Snapshot::find(const QString &schema, const QString &name) const
{
    const auto lookIn = [&](const DatabaseInfo &d) -> const SchemaObject * {
        for (const SchemaObject &o : d.objects) {
            if ((o.kind == ObjectKind::Table || o.kind == ObjectKind::VirtualTable
                 || o.kind == ObjectKind::View)
                && o.name.compare(name, Qt::CaseInsensitive) == 0)
                return &o;
        }
        return nullptr;
    };
    if (!schema.isEmpty()) {
        const DatabaseInfo *d = database(schema);
        return d ? lookIn(*d) : nullptr;
    }
    // SQLite looks in temp first, then main, then the attached ones.
    if (const DatabaseInfo *temp = database(QStringLiteral("temp")))
        if (const SchemaObject *o = lookIn(*temp))
            return o;
    for (const DatabaseInfo &d : databases) {
        if (d.name == QLatin1String("temp"))
            continue;
        if (const SchemaObject *o = lookIn(d))
            return o;
    }
    return nullptr;
}

std::vector<const SchemaObject *> Snapshot::belongingTo(const QString &schema, const QString &table,
                                                        ObjectKind kind) const
{
    std::vector<const SchemaObject *> out;
    if (const DatabaseInfo *d = database(schema)) {
        for (const SchemaObject &o : d->objects) {
            if (o.kind == kind && o.table.compare(table, Qt::CaseInsensitive) == 0)
                out.push_back(&o);
        }
    }
    return out;
}

SnapshotPtr loadSnapshot(sqlite3 *db)
{
    if (!db)
        return nullptr;
    auto snapshot = std::make_shared<Snapshot>();
    QString error;

    forEachRow(db, "PRAGMA database_list", {}, [&](sqlite3_stmt *s) {
        DatabaseInfo d;
        d.name = columnText(s, 1);
        d.file = columnText(s, 2);
        d.readOnly = sqlite3_db_readonly(db, d.name.toUtf8().constData()) == 1;
        snapshot->databases.push_back(std::move(d));
    });

    for (DatabaseInfo &d : snapshot->databases) {
        // What kind each table is, and how it is stored.
        struct ListEntry
        {
            QString type;
            bool withoutRowid = false;
            bool strict = false;
        };
        QHash<QString, ListEntry> listed;
        forEachRow(db, "SELECT name, type, wr, strict FROM pragma_table_list WHERE schema = ?1",
                   {d.name}, [&](sqlite3_stmt *s) {
                       listed.insert(columnText(s, 0).toLower(),
                                     {columnText(s, 1), sqlite3_column_int(s, 2) != 0,
                                      sqlite3_column_int(s, 3) != 0});
                   });

        const QByteArray master
            = (sqlite::quoteIdentifier(d.name) + QLatin1String(".sqlite_schema")).toUtf8();
        forEachRow(db, "SELECT type, name, tbl_name, sql FROM " + master, {}, [&](sqlite3_stmt *s) {
            SchemaObject o;
            o.schema = d.name;
            const QString type = columnText(s, 0);
            o.name = columnText(s, 1);
            o.table = columnText(s, 2);
            o.sql = columnText(s, 3);
            o.system = o.name.startsWith(QLatin1String("sqlite_"), Qt::CaseInsensitive);
            if (type == QLatin1String("table")) {
                const ListEntry entry = listed.value(o.name.toLower());
                o.kind = entry.type == QLatin1String("virtual") ? ObjectKind::VirtualTable
                                                                : ObjectKind::Table;
                o.system = o.system || entry.type == QLatin1String("shadow");
                o.withoutRowid = entry.withoutRowid;
                o.strict = entry.strict;
                o.autoincrement = hasAutoincrement(o.sql);
            } else if (type == QLatin1String("view")) {
                o.kind = ObjectKind::View;
            } else if (type == QLatin1String("index")) {
                o.kind = ObjectKind::Index;
            } else if (type == QLatin1String("trigger")) {
                o.kind = ObjectKind::Trigger;
            } else {
                return;
            }
            d.objects.push_back(std::move(o));
        });

        for (SchemaObject &o : d.objects) {
            if (o.kind != ObjectKind::Table && o.kind != ObjectKind::VirtualTable
                && o.kind != ObjectKind::View)
                continue;
            o.error = forEachRow(db,
                                 "SELECT cid, name, type, \"notnull\", dflt_value, pk, hidden "
                                 "FROM pragma_table_xinfo(?1, ?2)",
                                 {o.name, d.name}, [&](sqlite3_stmt *s) {
                                     ColumnInfo c;
                                     c.cid = sqlite3_column_int(s, 0);
                                     c.name = columnText(s, 1);
                                     c.type = columnText(s, 2);
                                     c.notNull = sqlite3_column_int(s, 3) != 0;
                                     c.hasDefault = sqlite3_column_type(s, 4) != SQLITE_NULL;
                                     c.defaultValue = columnText(s, 4);
                                     c.primaryKey = sqlite3_column_int(s, 5);
                                     c.hidden = sqlite3_column_int(s, 6);
                                     o.columns.push_back(std::move(c));
                                 });
            if (o.kind != ObjectKind::Table)
                continue;
            std::map<int, ForeignKey> keys;
            forEachRow(db,
                       "SELECT id, \"table\", \"from\", \"to\", on_update, on_delete FROM "
                       "pragma_foreign_key_list(?1, ?2) ORDER BY id, seq",
                       {o.name, d.name}, [&](sqlite3_stmt *s) {
                           ForeignKey &key = keys[sqlite3_column_int(s, 0)];
                           key.table = columnText(s, 1);
                           key.from << columnText(s, 2);
                           if (sqlite3_column_type(s, 3) != SQLITE_NULL)
                               key.to << columnText(s, 3);
                           key.onUpdate = columnText(s, 4);
                           key.onDelete = columnText(s, 5);
                       });
            // SQLite numbers them from the last one declared: reversed, they
            // come in the order of the CREATE TABLE.
            for (auto it = keys.rbegin(); it != keys.rend(); ++it)
                o.foreignKeys.push_back(std::move(it->second));
        }

        std::sort(d.objects.begin(), d.objects.end(),
                  [](const SchemaObject &a, const SchemaObject &b) {
                      if (kindOrder(a.kind) != kindOrder(b.kind))
                          return kindOrder(a.kind) < kindOrder(b.kind);
                      return a.name.compare(b.name, Qt::CaseInsensitive) < 0;
                  });
    }

    forEachRow(db, "SELECT DISTINCT lower(name) FROM pragma_function_list ORDER BY 1", {},
               [&](sqlite3_stmt *s) { snapshot->functions << columnText(s, 0); });
    forEachRow(db, "SELECT name FROM pragma_collation_list ORDER BY 1", {},
               [&](sqlite3_stmt *s) { snapshot->collations << columnText(s, 0); });
    return snapshot;
}

Affinity affinity(const QString &declaredType)
{
    // https://www.sqlite.org/datatype3.html#determination_of_column_affinity
    const QString t = declaredType.toUpper();
    if (t.contains(QLatin1String("INT")))
        return Affinity::Integer;
    if (t.contains(QLatin1String("CHAR")) || t.contains(QLatin1String("CLOB"))
        || t.contains(QLatin1String("TEXT")))
        return Affinity::Text;
    if (t.isEmpty() || t.contains(QLatin1String("BLOB")))
        return Affinity::Blob;
    if (t.contains(QLatin1String("REAL")) || t.contains(QLatin1String("FLOA"))
        || t.contains(QLatin1String("DOUB")))
        return Affinity::Real;
    return Affinity::Numeric;
}

} // namespace sqliteman::catalog
