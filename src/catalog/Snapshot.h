// SPDX-FileCopyrightText: 2026 Petr Vanek
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QMetaType>
#include <QString>
#include <QStringList>

#include <memory>
#include <vector>

struct sqlite3;

namespace sqliteman::catalog {

enum class ObjectKind {
    Database, // A schema: main, temp or an attached database.
    Table,
    VirtualTable,
    View,
    Index,
    Trigger,
    Column,
};

QString kindName(ObjectKind kind);

struct ColumnInfo
{
    int cid = 0;
    QString name;
    QString type; // As declared; empty for none.
    bool notNull = false;
    QString defaultValue; // The DEFAULT expression as written; empty for none.
    bool hasDefault = false;
    int primaryKey = 0; // Position in the primary key, from 1; 0 if not in it.
    // 0 a plain column, 1 hidden in a virtual table, 2 generated VIRTUAL,
    // 3 generated STORED.
    int hidden = 0;

    bool isGenerated() const { return hidden == 2 || hidden == 3; }
};

struct ForeignKey
{
    QString table; // The table referred to.
    QStringList from; // Columns of this table.
    QStringList to; // Columns of that one; empty: its primary key.
    QString onUpdate;
    QString onDelete;
};

struct SchemaObject
{
    ObjectKind kind = ObjectKind::Table;
    QString schema;
    QString name;
    QString table; // What an index or trigger belongs to; for tables, the name.
    QString sql; // From sqlite_schema; empty for automatic indexes.
    // Made by SQLite itself: sqlite_sequence, sqlite_stat1, automatic indexes,
    // the shadow tables of virtual tables.
    bool system = false;
    bool withoutRowid = false;
    bool strict = false;
    bool autoincrement = false;
    std::vector<ColumnInfo> columns; // Tables and views; empty if they could not be read.
    std::vector<ForeignKey> foreignKeys; // A table's, as its CREATE TABLE has them.
    QString error; // Why the columns could not be read, e.g. a missing module.

    bool hasRowid() const { return (kind == ObjectKind::Table) && !withoutRowid; }
    std::vector<QString> primaryKey() const;
};

struct DatabaseInfo
{
    QString name; // main, temp or the attached name.
    QString file; // Empty for temp and in-memory databases.
    bool readOnly = false;
    std::vector<SchemaObject> objects; // Sorted by kind, then name.
};

// Everything about one connection's databases that the browser and
// completion need. The loader builds a new snapshot and swaps the pointer;
// readers, including completion threads, never lock.
struct Snapshot
{
    std::vector<DatabaseInfo> databases; // main first, then temp, then attached.
    QStringList functions; // Lowercase, sorted.
    QStringList collations;

    const DatabaseInfo *database(const QString &name) const;
    // A table or view; an empty schema searches temp, main and the
    // attached databases in the order SQLite resolves names.
    const SchemaObject *find(const QString &schema, const QString &name) const;
    // Every object of a kind belonging to a table, like its indexes.
    std::vector<const SchemaObject *> belongingTo(const QString &schema, const QString &table,
                                                  ObjectKind kind) const;
};

using SnapshotPtr = std::shared_ptr<const Snapshot>;

// Reads it on the connection's thread. An object that cannot be described,
// such as a virtual table of a module not loaded, is listed with an error
// rather than failing the rest.
SnapshotPtr loadSnapshot(sqlite3 *db);

// What the column's declared type gives it: SQLite's affinity rules.
enum class Affinity { Integer, Text, Blob, Real, Numeric };
Affinity affinity(const QString &declaredType);

} // namespace sqliteman::catalog

Q_DECLARE_METATYPE(sqliteman::catalog::SnapshotPtr)
