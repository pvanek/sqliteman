// SPDX-FileCopyrightText: 2007-2026 Petr Vanek
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QString>
#include <QStringList>

#include <optional>
#include <vector>

namespace sqliteman::catalog {

// One column of a CREATE TABLE, with the constraints a dialog can edit
// taken apart. Whatever else the definition says stays in extra, as
// written, so editing a table never loses a clause this program does not
// know.
struct ColumnDef
{
    QString name;
    QString type; // Empty for none, which SQLite allows.
    bool primaryKey = false;
    bool autoincrement = false; // Only with primaryKey and the type INTEGER.
    bool notNull = false;
    bool unique = false;
    QString defaultValue; // The expression as it goes into SQL; empty for none.
    QString check; // The expression inside CHECK ( ); empty for none.
    QString collate;
    QString extra; // REFERENCES, GENERATED, named and conflict-clause constraints.

    // When altering: the column's name in the table as it is; empty for a
    // column being added.
    QString originalName;

    bool operator==(const ColumnDef &) const = default;
    // The same definition, whatever the column is called.
    bool sameDefinition(const ColumnDef &other) const;
};

struct TableDef
{
    QString schema; // main, temp or an attached database; empty: main.
    QString name;
    bool ifNotExists = false;
    bool withoutRowid = false;
    bool strict = false;
    std::vector<ColumnDef> columns;
    // Table constraints as written, without the commas between them:
    // PRIMARY KEY (a, b), UNIQUE (c), CHECK (...), FOREIGN KEY ...
    QStringList constraints;

    bool operator==(const TableDef &) const = default;
};

// The column as it goes inside CREATE TABLE ( ).
QString columnDefinition(const ColumnDef &column);
// CREATE TABLE, one column or constraint per line.
QString createTableStatement(const TableDef &table);

// Takes a CREATE TABLE statement apart, as sqlite_schema keeps it. Nullopt
// for what is not one with columns: CREATE TABLE ... AS SELECT, CREATE
// VIRTUAL TABLE, or text that does not parse. schema is not in the stored
// SQL, so it is left empty.
std::optional<TableDef> parseCreateTable(const QString &sql);

// What the user typed as a column's default, as an expression for DEFAULT:
// numbers, literals in quotes, parenthesized expressions and keywords like
// CURRENT_TIMESTAMP stay as they are; anything else becomes a string.
QString defaultExpression(const QString &userText);

// UNIQUE (a, b) and the like, for a dialog that adds a table constraint
// from a list of columns.
QString columnsConstraint(const QString &keyword, const QStringList &columns);

} // namespace sqliteman::catalog
