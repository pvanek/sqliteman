// SPDX-FileCopyrightText: 2007-2026 Petr Vanek
// SPDX-License-Identifier: GPL-3.0-or-later

#include "catalog/Pragmas.h"

#include "sql/Keywords.h"
#include "sqlite/Quote.h"

#include <QCoreApplication>

namespace sqliteman::catalog {

namespace {

QString tr(const char *text)
{
    return QCoreApplication::translate("Pragmas", text);
}

QString prefix(const PragmaInfo &p, const QString &schema)
{
    return p.perDatabase && !schema.isEmpty() ? sql::identifier(schema) + QLatin1Char('.')
                                              : QString();
}

} // namespace

const std::vector<PragmaInfo> &pragmas()
{
    using K = PragmaInfo::Kind;
    static const std::vector<PragmaInfo> list = [] {
        const auto s = [](const char *text) { return QString::fromLatin1(text); };
        return std::vector<PragmaInfo> {
            {s("application_id"),
             K::Integer,
             true,
             {},
             tr("A number that tells which program the file belongs to.")},
            {s("auto_vacuum"),
             K::Choice,
             true,
             {s("NONE"), s("FULL"), s("INCREMENTAL")},
             tr("Whether free pages are given back to the file system. Changing it between NONE "
                "and the others takes a VACUUM.")},
            {s("automatic_index"),
             K::Boolean,
             false,
             {},
             tr("Whether SQLite may build temporary indexes for a query.")},
            {s("busy_timeout"),
             K::Integer,
             false,
             {},
             tr("Milliseconds to wait for another program's lock.")},
            {s("cache_size"),
             K::Integer,
             true,
             {},
             tr("Pages kept in memory; negative: kibibytes.")},
            {s("cell_size_check"),
             K::Boolean,
             false,
             {},
             tr("Extra checks of every page read, against corrupt files.")},
            {s("defer_foreign_keys"),
             K::Boolean,
             false,
             {},
             tr("Foreign keys checked at COMMIT rather than at each statement.")},
            {s("encoding"),
             K::ReadOnly,
             false,
             {},
             tr("The text encoding; set only before the first table is made.")},
            {s("foreign_keys"),
             K::Boolean,
             false,
             {},
             tr("Whether foreign key constraints are enforced. Off by default in SQLite.")},
            {s("freelist_count"), K::ReadOnly, true, {}, tr("Unused pages in the file.")},
            {s("fullfsync"), K::Boolean, false, {}, tr("macOS: F_FULLFSYNC for every sync.")},
            {s("ignore_check_constraints"),
             K::Boolean,
             false,
             {},
             tr("CHECK constraints not enforced.")},
            {s("journal_mode"),
             K::Choice,
             true,
             {s("DELETE"), s("TRUNCATE"), s("PERSIST"), s("MEMORY"), s("WAL"), s("OFF")},
             tr("How transactions are made safe. WAL lets readers and a writer work at the same "
                "time.")},
            {s("journal_size_limit"),
             K::Integer,
             true,
             {},
             tr("Bytes of journal kept after a transaction; -1 for no limit.")},
            {s("legacy_alter_table"),
             K::Boolean,
             false,
             {},
             tr("ALTER TABLE RENAME as before SQLite 3.26: references are left alone.")},
            {s("locking_mode"),
             K::Choice,
             true,
             {s("NORMAL"), s("EXCLUSIVE")},
             tr("EXCLUSIVE keeps the lock until the database is closed.")},
            {s("max_page_count"), K::Integer, true, {}, tr("The most pages the file may grow to.")},
            {s("mmap_size"),
             K::Integer,
             true,
             {},
             tr("Bytes of the file read through memory mapping.")},
            {s("page_count"), K::ReadOnly, true, {}, tr("Pages in the file.")},
            {s("page_size"),
             K::Integer,
             true,
             {},
             tr("Bytes per page; changes with the next VACUUM.")},
            {s("query_only"), K::Boolean, false, {}, tr("Every change to the database refused.")},
            {s("read_uncommitted"),
             K::Boolean,
             false,
             {},
             tr("With a shared cache: read what others have not committed.")},
            {s("recursive_triggers"),
             K::Boolean,
             false,
             {},
             tr("Triggers may fire triggers of the same table.")},
            {s("reverse_unordered_selects"),
             K::Boolean,
             false,
             {},
             tr("Rows without ORDER BY in reverse: finds queries that rely on an order by "
                "chance.")},
            {s("secure_delete"),
             K::Choice,
             true,
             {s("OFF"), s("ON"), s("FAST")},
             tr("Deleted content overwritten with zeros.")},
            {s("synchronous"),
             K::Choice,
             true,
             {s("OFF"), s("NORMAL"), s("FULL"), s("EXTRA")},
             tr("How hard SQLite makes sure data reached the disk.")},
            {s("temp_store"),
             K::Choice,
             false,
             {s("DEFAULT"), s("FILE"), s("MEMORY")},
             tr("Where temporary tables and indexes go.")},
            {s("trusted_schema"),
             K::Boolean,
             false,
             {},
             tr("Functions and virtual tables may run from views and triggers.")},
            {s("user_version"),
             K::Integer,
             true,
             {},
             tr("A number for the program's own use, often the schema's version.")},
            {s("wal_autocheckpoint"),
             K::Integer,
             false,
             {},
             tr("Pages in the WAL that trigger a checkpoint.")},
        };
    }();
    return list;
}

const PragmaInfo *pragma(const QString &name)
{
    for (const PragmaInfo &p : pragmas()) {
        if (p.name == name)
            return &p;
    }
    return nullptr;
}

QByteArray readPragmasQuery(const QString &schema)
{
    QString sql;
    for (const PragmaInfo &p : pragmas())
        sql += QLatin1String("PRAGMA ") + prefix(p, schema) + p.name + QLatin1String(";\n");
    return sql.toUtf8();
}

QByteArray setPragmaStatement(const QString &schema, const PragmaInfo &p, const QString &value)
{
    QString v = value.trimmed();
    switch (p.kind) {
    case PragmaInfo::Kind::Boolean: {
        const QString lower = v.toLower();
        v = (lower == QLatin1String("1") || lower == QLatin1String("on")
             || lower == QLatin1String("true") || lower == QLatin1String("yes"))
            ? QStringLiteral("ON")
            : QStringLiteral("OFF");
        break;
    }
    case PragmaInfo::Kind::Integer:
        v = QString::number(v.toLongLong());
        break;
    case PragmaInfo::Kind::Choice: {
        bool number = false;
        const int index = v.toInt(&number);
        if (number && index >= 0 && index < p.choices.size())
            v = p.choices.at(index);
        if (!p.choices.contains(v, Qt::CaseInsensitive))
            v = p.choices.value(0);
        break;
    }
    case PragmaInfo::Kind::Text:
        v = sqlite::quoteLiteral(v);
        break;
    case PragmaInfo::Kind::ReadOnly:
        return {};
    }
    return (QLatin1String("PRAGMA ") + prefix(p, schema) + p.name + QLatin1String(" = ") + v)
        .toUtf8();
}

QString displayValue(const PragmaInfo &p, const QString &raw)
{
    if (p.kind == PragmaInfo::Kind::Boolean)
        return raw == QLatin1String("1") ? QStringLiteral("ON") : QStringLiteral("OFF");
    if (p.kind == PragmaInfo::Kind::Choice) {
        bool number = false;
        const int index = raw.toInt(&number);
        if (number && index >= 0 && index < p.choices.size())
            return QStringLiteral("%1 (%2)").arg(p.choices.at(index), raw);
        return raw.toUpper();
    }
    return raw;
}

} // namespace sqliteman::catalog
