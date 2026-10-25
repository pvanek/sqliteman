// SPDX-FileCopyrightText: 2026 Petr Vanek
// SPDX-License-Identifier: GPL-3.0-or-later

#include "catalog/Editing.h"

#include "sql/Keywords.h"
#include "sql/Lexer.h"
#include "sqlite/Direct.h"
#include "sqlite/Handles.h"
#include "sqlite/Quote.h"

#include <QCoreApplication>
#include <QSet>

namespace sqliteman::catalog {

namespace {

QString tr(const char *text)
{
    return QCoreApplication::translate("Editing", text);
}

bool isRowidName(const QString &name)
{
    return name.compare(QLatin1String("rowid"), Qt::CaseInsensitive) == 0
        || name.compare(QLatin1String("_rowid_"), Qt::CaseInsensitive) == 0
        || name.compare(QLatin1String("oid"), Qt::CaseInsensitive) == 0;
}

// The single INTEGER PRIMARY KEY column, which is the rowid under another name.
const ColumnInfo *rowidAlias(const SchemaObject &table)
{
    const ColumnInfo *pk = nullptr;
    for (const ColumnInfo &c : table.columns) {
        if (c.primaryKey == 0)
            continue;
        if (pk)
            return nullptr;
        pk = &c;
    }
    return pk && pk->type.compare(QLatin1String("INTEGER"), Qt::CaseInsensitive) == 0 ? pk
                                                                                      : nullptr;
}

QByteArray identifier(const QString &name)
{
    return sql::identifier(name).toUtf8();
}

} // namespace

EditTarget editTarget(const sqlite::RowStore &rows, const Snapshot &snapshot)
{
    EditTarget target;
    QSet<std::pair<QString, QString>> tables;
    for (int c = 0; c < rows.columnCount(); ++c) {
        const sqlite::Column &column = rows.column(c);
        if (!column.table.isEmpty())
            tables.insert({column.database.toLower(), column.table.toLower()});
    }
    if (tables.size() != 1) {
        target.reason = tables.isEmpty() ? tr("The result does not come from a table.")
                                         : tr("The result has columns from several tables.");
        return target;
    }
    QString database;
    QString tableName;
    for (int c = 0; c < rows.columnCount(); ++c) {
        if (!rows.column(c).table.isEmpty()) {
            database = rows.column(c).database;
            tableName = rows.column(c).table;
            break;
        }
    }
    const SchemaObject *table = snapshot.find(database, tableName);
    if (!table) {
        target.reason = tr("The table %1 is not known; refresh the database.").arg(tableName);
        return target;
    }
    if (table->kind != ObjectKind::Table) {
        target.reason = tr("%1 is a %2, not a table.").arg(table->name, kindName(table->kind));
        return target;
    }
    const DatabaseInfo *databaseInfo = snapshot.database(database);
    if (databaseInfo && databaseInfo->readOnly) {
        target.reason = tr("The database %1 is read-only.").arg(database);
        return target;
    }
    target.schema = table->schema;
    target.table = table->name;

    const std::size_t count = std::size_t(rows.columnCount());
    target.columns.resize(count);
    target.readOnly.resize(count);
    target.affinity.resize(count, Affinity::Blob);
    int rowidColumn = -1;
    const ColumnInfo *alias = rowidAlias(*table);
    for (int c = 0; c < rows.columnCount(); ++c) {
        const sqlite::Column &column = rows.column(c);
        if (column.table.isEmpty())
            continue;
        const ColumnInfo *info = nullptr;
        for (const ColumnInfo &ci : table->columns) {
            if (ci.name.compare(column.origin, Qt::CaseInsensitive) == 0)
                info = &ci;
        }
        if (!info) {
            // The rowid itself, as SQLite reports it for a table without an
            // INTEGER PRIMARY KEY.
            if (table->hasRowid() && isRowidName(column.origin) && rowidColumn < 0) {
                target.columns[std::size_t(c)] = QStringLiteral("rowid");
                target.readOnly[std::size_t(c)] = true;
                target.affinity[std::size_t(c)] = Affinity::Integer;
                rowidColumn = c;
            }
            continue;
        }
        target.columns[std::size_t(c)] = info->name;
        target.readOnly[std::size_t(c)] = info->isGenerated();
        target.affinity[std::size_t(c)] = affinity(info->type);
        if (alias && info == alias && rowidColumn < 0)
            rowidColumn = c;
    }

    if (rowidColumn >= 0) {
        target.key = {rowidColumn};
        target.keyIsRowid = true;
        return target;
    }
    const std::vector<QString> primaryKey = table->primaryKey();
    if (primaryKey.empty()) {
        target.reason
            = tr("%1 has no primary key, and the result does not have its rowid.").arg(table->name);
        return target;
    }
    QStringList missing;
    std::vector<int> key;
    for (const QString &name : primaryKey) {
        int found = -1;
        for (int c = 0; c < rows.columnCount() && found < 0; ++c) {
            if (target.columns[std::size_t(c)].compare(name, Qt::CaseInsensitive) == 0)
                found = c;
        }
        if (found < 0)
            missing << name;
        else
            key.push_back(found);
    }
    if (!missing.isEmpty()) {
        target.reason = tr("The result lacks the primary key column(s) %1.")
                            .arg(missing.join(QLatin1String(", ")));
        return target;
    }
    target.key = std::move(key);
    return target;
}

sqlite::Value typedValue(const QString &text, Affinity affinity)
{
    switch (affinity) {
    case Affinity::Integer:
    case Affinity::Real:
    case Affinity::Numeric:
    case Affinity::Blob:
        return sqlite::Value::fromUserText(text);
    case Affinity::Text:
        break;
    }
    return sqlite::Value::text(text);
}

QString DmlStatement::preview() const
{
    // Puts the values in where their parameters are, as literals.
    const QByteArrayView text(sql);
    QString out;
    qsizetype at = 0;
    for (const sql::Token &t : sql::tokenize(text)) {
        if (t.kind != sql::TokenKind::Parameter)
            continue;
        out += QString::fromUtf8(text.sliced(at, t.offset - at));
        const int index = text.sliced(t.offset + 1, t.length - 1).toInt() - 1;
        if (index >= 0 && index < int(parameters.size())) {
            const sqlite::Value &v = parameters[std::size_t(index)];
            QString literal = sqlite::literal(v);
            constexpr int Longest = 80;
            if (literal.size() > Longest)
                literal = literal.left(Longest) + QStringLiteral("…");
            out += literal;
        }
        at = t.end();
    }
    out += QString::fromUtf8(text.sliced(at));
    return out;
}

std::vector<DmlStatement> dmlStatements(const EditTarget &target, const std::vector<RowEdit> &edits)
{
    const QByteArray table = identifier(target.schema) + '.' + identifier(target.table);
    std::vector<DmlStatement> out;

    for (const RowEdit &e : edits) {
        DmlStatement s;
        const auto parameter = [&s](const sqlite::Value &v) {
            s.parameters.push_back(v);
            return '?' + QByteArray::number(qsizetype(s.parameters.size()));
        };
        const auto where = [&] {
            QByteArrayList conditions;
            for (std::size_t i = 0; i < target.key.size() && i < e.key.size(); ++i) {
                const QByteArray name = target.keyIsRowid
                    ? QByteArray("rowid")
                    : identifier(target.columns[std::size_t(target.key[i])]);
                // IS: an old table may have NULL in a primary key column.
                conditions << name + " IS " + parameter(e.key[i]);
            }
            return " WHERE " + conditions.join(" AND ");
        };

        switch (e.kind) {
        case RowEdit::Kind::Update: {
            QByteArrayList sets;
            for (const auto &[column, value] : e.values)
                sets << identifier(target.columns[std::size_t(column)]) + " = " + parameter(value);
            if (sets.isEmpty())
                continue;
            s.sql = "UPDATE " + table + " SET " + sets.join(", ");
            s.sql += where();
            break;
        }
        case RowEdit::Kind::Insert: {
            QByteArrayList names, values;
            for (const auto &[column, value] : e.values) {
                names << identifier(target.columns[std::size_t(column)]);
                values << parameter(value);
            }
            s.sql = names.isEmpty() ? "INSERT INTO " + table + " DEFAULT VALUES"
                                    : "INSERT INTO " + table + " (" + names.join(", ")
                    + ") VALUES (" + values.join(", ") + ")";
            break;
        }
        case RowEdit::Kind::Delete:
            s.sql = "DELETE FROM " + table;
            s.sql += where();
            break;
        }
        out.push_back(std::move(s));
    }
    return out;
}

sqlite::TaskOutcome applyEdits(sqlite3 *db, const std::vector<DmlStatement> &statements)
{
    sqlite::TaskOutcome outcome;
    outcome.error = sqlite::exec(db, "SAVEPOINT sqliteman_edits");
    if (!outcome.error.isEmpty())
        return outcome;
    int done = 0;
    for (const DmlStatement &s : statements) {
        sqlite3_stmt *raw = nullptr;
        if (sqlite3_prepare_v2(db, s.sql.constData(), int(s.sql.size()), &raw, nullptr)
            != SQLITE_OK) {
            outcome.error = QString::fromUtf8(sqlite3_errmsg(db));
            break;
        }
        sqlite::StatementPtr stmt(raw);
        if (sqlite::bindValues(stmt.get(), s.parameters) != SQLITE_OK) {
            outcome.error = QString::fromUtf8(sqlite3_errmsg(db));
            break;
        }
        if (sqlite3_step(stmt.get()) != SQLITE_DONE) {
            outcome.error = QString::fromUtf8(sqlite3_errmsg(db));
            break;
        }
        if (sqlite3_changes(db) != 1) {
            outcome.error = tr("%1 changed %2 rows instead of one; nothing was saved.")
                                .arg(s.preview())
                                .arg(sqlite3_changes(db));
            break;
        }
        ++done;
    }
    if (outcome.error.isEmpty()) {
        outcome.error = sqlite::exec(db, "RELEASE sqliteman_edits");
    } else {
        sqlite::exec(db, "ROLLBACK TO sqliteman_edits");
        sqlite::exec(db, "RELEASE sqliteman_edits");
        done = 0;
    }
    outcome.value = done;
    return outcome;
}

} // namespace sqliteman::catalog
