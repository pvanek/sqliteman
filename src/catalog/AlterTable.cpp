// SPDX-FileCopyrightText: 2007-2026 Petr Vanek
// SPDX-License-Identifier: GPL-3.0-or-later

#include "catalog/AlterTable.h"

#include "sql/Keywords.h"
#include "sql/Lexer.h"
#include "sqlite/Direct.h"
#include "sqlite/Quote.h"

#include <QCoreApplication>

#include <sqlite3.h>

namespace sqliteman::catalog {

namespace {

QString tr(const char *text)
{
    return QCoreApplication::translate("AlterTable", text);
}

QString qualified(const QString &schema, const QString &name)
{
    return sql::identifier(schema.isEmpty() ? QStringLiteral("main") : schema) + QLatin1Char('.')
        + sql::identifier(name);
}

const ColumnDef *columnNamed(const TableDef &table, const QString &name)
{
    for (const ColumnDef &c : table.columns) {
        if (c.name.compare(name, Qt::CaseInsensitive) == 0)
            return &c;
    }
    return nullptr;
}

// Whether a name appears in SQL as a name, not in a string or comment.
bool mentions(const QString &sql, const QString &name)
{
    const QByteArray utf8 = sql.toUtf8();
    for (const sql::Token &t : sql::tokenize(utf8)) {
        if (t.kind != sql::TokenKind::Identifier && t.kind != sql::TokenKind::QuotedIdentifier
            && t.kind != sql::TokenKind::Keyword)
            continue;
        const QByteArray token = sql::unquote(QByteArrayView(utf8).sliced(t.offset, t.length));
        if (QString::fromUtf8(token).compare(name, Qt::CaseInsensitive) == 0)
            return true;
    }
    return false;
}

// Whether ALTER TABLE ADD COLUMN takes the column; its rules are in the
// documentation of ALTER TABLE.
bool canAdd(const ColumnDef &c)
{
    if (c.primaryKey || c.unique)
        return false;
    const QString def = c.defaultValue.trimmed();
    if (def.startsWith(QLatin1Char('('))
        || def.startsWith(QLatin1String("CURRENT_"), Qt::CaseInsensitive))
        return false;
    if (c.notNull
        && (def.isEmpty() || def.compare(QLatin1String("NULL"), Qt::CaseInsensitive) == 0))
        return false;
    if (c.extra.contains(QLatin1String("STORED"), Qt::CaseInsensitive))
        return false;
    if (c.extra.contains(QLatin1String("REFERENCES"), Qt::CaseInsensitive) && !def.isEmpty()
        && def.compare(QLatin1String("NULL"), Qt::CaseInsensitive) != 0)
        return false;
    return true;
}

bool isGenerated(const ColumnDef &c)
{
    return c.extra.contains(QLatin1String("GENERATED"), Qt::CaseInsensitive)
        || c.extra.trimmed().startsWith(QLatin1String("AS"), Qt::CaseInsensitive)
        || c.extra.contains(QLatin1String(" AS ("), Qt::CaseInsensitive)
        || c.extra.contains(QLatin1String(" AS("), Qt::CaseInsensitive);
}

} // namespace

QString renameIdentifier(const QString &sql, const QString &from, const QString &to)
{
    const QByteArray utf8 = sql.toUtf8();
    QByteArray out;
    qsizetype at = 0;
    for (const sql::Token &t : sql::tokenize(utf8)) {
        if (t.kind != sql::TokenKind::Identifier && t.kind != sql::TokenKind::QuotedIdentifier
            && t.kind != sql::TokenKind::Keyword)
            continue;
        const QByteArray name = sql::unquote(QByteArrayView(utf8).sliced(t.offset, t.length));
        if (QString::fromUtf8(name).compare(from, Qt::CaseInsensitive) != 0)
            continue;
        out += utf8.mid(at, t.offset - at);
        out += sql::identifier(to).toUtf8();
        at = t.end();
    }
    out += utf8.mid(at);
    return QString::fromUtf8(out);
}

QString qualifyCreate(const QString &sql, const QString &schema)
{
    const QByteArray utf8 = sql.toUtf8();
    std::vector<sql::Token> tokens;
    for (const sql::Token &t : sql::tokenize(utf8)) {
        if (sql::isSignificant(t.kind))
            tokens.push_back(t);
    }
    const auto is = [&](std::size_t i, const char *word) {
        return i < tokens.size()
            && QByteArrayView(utf8)
                   .sliced(tokens[i].offset, tokens[i].length)
                   .compare(QByteArrayView(word), Qt::CaseInsensitive)
            == 0;
    };
    std::size_t i = 0;
    if (!is(i++, "CREATE"))
        return sql;
    while (is(i, "UNIQUE") || is(i, "TEMP") || is(i, "TEMPORARY"))
        ++i;
    if (!is(i, "INDEX") && !is(i, "TRIGGER"))
        return sql;
    ++i;
    if (is(i, "IF") && is(i + 1, "NOT") && is(i + 2, "EXISTS"))
        i += 3;
    if (i >= tokens.size())
        return sql;
    // Already qualified.
    if (i + 1 < tokens.size() && utf8[tokens[i + 1].offset] == '.')
        return sql;
    const qsizetype at = tokens[i].offset;
    return QString::fromUtf8(utf8.left(at)) + sql::identifier(schema) + QLatin1Char('.')
        + QString::fromUtf8(utf8.mid(at));
}

QString AlterPlan::preview() const
{
    QStringList lines;
    if (rebuild)
        lines << QStringLiteral("PRAGMA foreign_keys = OFF;");
    lines << QStringLiteral("BEGIN;");
    for (const QString &s : renames)
        lines << s + QLatin1Char(';');
    for (const QString &s : statements)
        lines << s + QLatin1Char(';');
    if (rebuild)
        lines << QStringLiteral("PRAGMA foreign_key_check;");
    lines << QStringLiteral("COMMIT;");
    if (rebuild)
        lines << QStringLiteral("PRAGMA foreign_keys = ON;");
    QString out;
    for (const QString &w : warnings)
        out += QLatin1String("-- ") + w + QLatin1Char('\n');
    return out + lines.join(QLatin1Char('\n'));
}

AlterPlan planAlterTable(const AlterRequest &request, const QStringList &indexSql,
                         const QStringList &triggerSql)
{
    const TableDef &before = request.before;
    const TableDef &after = request.after;
    const QString schema = before.schema.isEmpty() ? QStringLiteral("main") : before.schema;
    AlterPlan plan;

    // Renames first: the table, then its columns.
    QString name = before.name;
    if (after.name != before.name) {
        plan.renames << QStringLiteral("ALTER TABLE %1 RENAME TO %2")
                            .arg(qualified(schema, name), sql::identifier(after.name));
        name = after.name;
    }
    for (const ColumnDef &c : after.columns) {
        if (!c.originalName.isEmpty() && c.originalName != c.name)
            plan.renames << QStringLiteral("ALTER TABLE %1 RENAME COLUMN %2 TO %3")
                                .arg(qualified(schema, name), sql::identifier(c.originalName),
                                     sql::identifier(c.name));
    }

    // The table with its columns renamed, the starting point from here on.
    std::vector<ColumnDef> kept;
    std::vector<const ColumnDef *> added;
    QStringList dropped;
    for (const ColumnDef &c : after.columns) {
        if (c.originalName.isEmpty())
            added.push_back(&c);
        else
            kept.push_back(c);
    }
    for (const ColumnDef &old : before.columns) {
        bool found = false;
        for (const ColumnDef &c : after.columns)
            found = found || c.originalName.compare(old.name, Qt::CaseInsensitive) == 0;
        if (!found)
            dropped << old.name;
    }

    bool native = before.constraints == after.constraints
        && before.withoutRowid == after.withoutRowid && before.strict == after.strict;
    // Kept columns in their old order, each defined as it was, and the new
    // ones after them all.
    int previousOld = -1;
    bool seenAdded = false;
    for (const ColumnDef &c : after.columns) {
        if (c.originalName.isEmpty()) {
            seenAdded = true;
            continue;
        }
        const ColumnDef *old = columnNamed(before, c.originalName);
        const int index = old ? int(old - before.columns.data()) : -1;
        if (!old || !old->sameDefinition(c) || seenAdded || index < previousOld)
            native = false;
        previousOld = index;
    }
    for (const ColumnDef *c : added)
        native = native && canAdd(*c);
    for (const QString &d : dropped) {
        const ColumnDef *old = columnNamed(before, d);
        if (old && (old->primaryKey || old->unique))
            native = false;
        for (const QString &s : indexSql)
            native = native && !mentions(s, d);
        for (const QString &s : before.constraints)
            native = native && !mentions(s, d);
        for (const ColumnDef &c : kept)
            native = native && !mentions(c.extra + QLatin1Char(' ') + c.check, d);
    }

    if (native) {
        for (const QString &d : dropped)
            plan.statements << QStringLiteral("ALTER TABLE %1 DROP COLUMN %2")
                                   .arg(qualified(schema, name), sql::identifier(d));
        for (const ColumnDef *c : added)
            plan.statements << QStringLiteral("ALTER TABLE %1 ADD COLUMN %2")
                                   .arg(qualified(schema, name), columnDefinition(*c));
        return plan;
    }

    plan.rebuild = true;
    // A name nothing else has, for the new table while the old one is there.
    const QString temporary = QStringLiteral("sqliteman_new_") + name;
    TableDef created = after;
    created.schema = schema;
    created.name = temporary;
    created.ifNotExists = false;
    for (ColumnDef &c : created.columns)
        c.originalName.clear();

    QStringList into;
    QStringList from;
    for (const ColumnDef &c : kept) {
        // A generated column takes no value; it is computed again.
        if (isGenerated(c))
            continue;
        into << sql::identifier(c.name);
        from << sql::identifier(c.name);
    }
    // With legacy_alter_table, the rename at the end leaves views and
    // triggers alone instead of checking them against a table that is gone.
    plan.statements << QStringLiteral("PRAGMA legacy_alter_table = ON");
    plan.statements << createTableStatement(created);
    if (!into.isEmpty())
        plan.statements << QStringLiteral("INSERT INTO %1 (%2) SELECT %3 FROM %4")
                               .arg(qualified(schema, temporary), into.join(QLatin1String(", ")),
                                    from.join(QLatin1String(", ")), qualified(schema, name));
    plan.statements << QStringLiteral("DROP TABLE %1").arg(qualified(schema, name));
    plan.statements << QStringLiteral("ALTER TABLE %1 RENAME TO %2")
                           .arg(qualified(schema, temporary), sql::identifier(name));
    for (const QString &s : indexSql) {
        bool usable = true;
        for (const QString &d : dropped)
            usable = usable && !mentions(s, d);
        if (usable)
            plan.statements << qualifyCreate(s, schema);
        else
            plan.warnings << tr("An index on a dropped column is not made again: %1").arg(s);
    }
    for (const QString &s : triggerSql) {
        plan.statements << qualifyCreate(s, schema);
        for (const QString &d : dropped) {
            if (mentions(s, d))
                plan.warnings << tr("A trigger uses the dropped column %1 and will fail when it "
                                    "fires: %2")
                                     .arg(d, s.section(QLatin1Char('\n'), 0, 0));
        }
    }
    plan.statements << QStringLiteral("PRAGMA legacy_alter_table = OFF");
    return plan;
}

sqlite::TaskOutcome alterTable(sqlite3 *db, const AlterRequest &request)
{
    using sqlite::exec;
    sqlite::TaskOutcome outcome;
    QStringList log;
    const QString schema
        = request.before.schema.isEmpty() ? QStringLiteral("main") : request.before.schema;

    // The renames come first, so the indexes and triggers are read as they
    // are after them.
    const AlterPlan renames = planAlterTable(request, {}, {});
    const bool inTransaction = !sqlite3_get_autocommit(db);
    bool foreignKeys = false;
    sqlite::forEachRow(db, "PRAGMA foreign_keys", {},
                       [&](sqlite3_stmt *s) { foreignKeys = sqlite3_column_int(s, 0) != 0; });

    const auto run = [&](const QString &sql) {
        const QString error = exec(db, sql.toUtf8());
        log << sql + QLatin1Char(';');
        return error;
    };

    // Whether a rebuild is needed is known only from the indexes, but a
    // rebuild needs the foreign keys off, which SQLite allows only outside a
    // transaction: find out from the indexes as they are now, renamed.
    QStringList indexSql;
    QStringList triggerSql;
    const auto readDependents = [&](const QString &table) {
        indexSql.clear();
        triggerSql.clear();
        const QByteArray query = "SELECT type, sql FROM "
            + (sql::identifier(schema) + QLatin1String(".sqlite_schema")).toUtf8()
            + " WHERE tbl_name = ?1 COLLATE NOCASE AND type IN ('index', 'trigger') AND sql "
              "IS NOT NULL";
        return sqlite::forEachRow(db, query, {table}, [&](sqlite3_stmt *s) {
            (sqlite::columnText(s, 0) == QLatin1String("index") ? indexSql : triggerSql)
                << sqlite::columnText(s, 1);
        });
    };
    readDependents(request.before.name);
    QStringList renamedIndexes = indexSql;
    for (const ColumnDef &c : request.after.columns) {
        if (!c.originalName.isEmpty() && c.originalName != c.name)
            for (QString &s : renamedIndexes)
                s = renameIdentifier(s, c.originalName, c.name);
    }
    const bool rebuild = planAlterTable(request, renamedIndexes, triggerSql).rebuild;
    if (rebuild && foreignKeys && inTransaction) {
        outcome.error = tr("The table has to be rebuilt, and that needs foreign keys switched off, "
                           "which SQLite allows only outside a transaction. Commit or roll back "
                           "first.");
        return outcome;
    }

    QString error;
    if (rebuild && foreignKeys)
        exec(db, "PRAGMA foreign_keys = OFF");
    error = run(inTransaction ? QStringLiteral("SAVEPOINT sqliteman_alter")
                              : QStringLiteral("BEGIN"));
    for (const QString &s : renames.renames) {
        if (error.isEmpty())
            error = run(s);
    }
    AlterPlan plan;
    if (error.isEmpty()) {
        error = readDependents(request.after.name);
        if (error.isEmpty())
            plan = planAlterTable(request, indexSql, triggerSql);
    }
    for (const QString &s : plan.statements) {
        if (error.isEmpty())
            error = run(s);
    }
    if (error.isEmpty() && plan.rebuild && foreignKeys) {
        const QByteArray check = "PRAGMA " + sql::identifier(schema).toUtf8()
            + ".foreign_key_check(" + sqlite::quoteLiteral(request.after.name).toUtf8() + ")";
        int violations = 0;
        error = sqlite::forEachRow(db, check, {}, [&](sqlite3_stmt *) { ++violations; });
        if (error.isEmpty() && violations > 0)
            error = QCoreApplication::translate(
                "AlterTable", "%n row(s) would break a foreign key.", nullptr, violations);
    }
    if (error.isEmpty()) {
        error = run(inTransaction ? QStringLiteral("RELEASE sqliteman_alter")
                                  : QStringLiteral("COMMIT"));
    }
    if (!error.isEmpty()) {
        if (inTransaction) {
            exec(db, "ROLLBACK TO sqliteman_alter");
            exec(db, "RELEASE sqliteman_alter");
        } else if (!sqlite3_get_autocommit(db)) {
            exec(db, "ROLLBACK");
        }
    }
    exec(db, "PRAGMA legacy_alter_table = OFF");
    if (rebuild && foreignKeys)
        exec(db, "PRAGMA foreign_keys = ON");
    outcome.error = error;
    outcome.value = log;
    return outcome;
}

} // namespace sqliteman::catalog
