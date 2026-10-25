// SPDX-FileCopyrightText: 2026 Petr Vanek
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "catalog/Snapshot.h"

#include <QMetaType>
#include <QString>
#include <QStringList>

#include <utility>
#include <vector>

struct sqlite3;

namespace sqliteman::catalog {

struct IndexDetails
{
    QString name;
    bool unique = false;
    // How it came to be: CREATE INDEX, a UNIQUE constraint or the primary key.
    enum class Origin { Created, Unique, PrimaryKey } origin = Origin::Created;
    bool partial = false;
    QStringList columns; // With DESC and COLLATE where they are not the default.
    QString sql; // Empty for the automatic ones.
};

// Everything the object page shows about one table, view, index or trigger.
struct ObjectDetails
{
    SchemaObject object;
    std::vector<IndexDetails> indexes; // A table's, or the index itself.
    std::vector<ForeignKey> foreignKeys;
    std::vector<SchemaObject> triggers;
    QString error;
};

// Reads it on the connection's thread.
ObjectDetails loadDetails(sqlite3 *db, const QString &schema, const QString &name);

// Everything the page of a database shows: main, temp or an attached one.
struct DatabaseDetails
{
    DatabaseInfo database; // With its objects, as the snapshot has them.
    // What the file is and how it is set up, as a name and a value each:
    // its size, pages, encoding, journal mode, versions.
    std::vector<std::pair<QString, QString>> properties;
    QString error;
};

// Reads it on the connection's thread.
DatabaseDetails loadDatabaseDetails(sqlite3 *db, const QString &schema);

// Small DDL the browser's context menu runs.
QString dropStatement(ObjectKind kind, const QString &schema, const QString &name);
// CREATE INDEX on the columns given, each as it goes into SQL ("name DESC").
QString createIndexStatement(const QString &schema, const QString &name, const QString &table,
                             const QStringList &columns, bool unique, bool ifNotExists,
                             const QString &where = {});
// Templates for the dialogs that take SQL: a view on a query, a trigger on
// a table or view.
QString createViewStatement(const QString &schema, const QString &name, const QString &select);
QString triggerTemplate(const QString &schema, const QString &table, bool onView);

} // namespace sqliteman::catalog

Q_DECLARE_METATYPE(sqliteman::catalog::ObjectDetails)
Q_DECLARE_METATYPE(sqliteman::catalog::DatabaseDetails)
