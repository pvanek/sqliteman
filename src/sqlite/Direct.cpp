// SPDX-FileCopyrightText: 2026 Petr Vanek
// SPDX-License-Identifier: GPL-3.0-or-later

#include "sqlite/Direct.h"

#include "sqlite/Handles.h"
#include "sqlite/Value.h"

namespace sqliteman::sqlite {

int bindValues(sqlite3_stmt *stmt, const std::vector<Value> &values)
{
    const int count = sqlite3_bind_parameter_count(stmt);
    for (int i = 0; i < count; ++i) {
        const Value v = std::size_t(i) < values.size() ? values[std::size_t(i)] : Value();
        const QByteArray &d = v.data();
        int rc = SQLITE_OK;
        switch (v.type()) {
        case ValueType::Null:
            rc = sqlite3_bind_null(stmt, i + 1);
            break;
        case ValueType::Integer:
            rc = sqlite3_bind_int64(stmt, i + 1, v.toInteger());
            break;
        case ValueType::Real:
            rc = sqlite3_bind_double(stmt, i + 1, v.toReal());
            break;
        case ValueType::Text:
            rc = sqlite3_bind_text64(stmt, i + 1, d.constData(), sqlite3_uint64(d.size()),
                                     SQLITE_TRANSIENT, SQLITE_UTF8);
            break;
        case ValueType::Blob:
            // A null pointer would bind NULL rather than an empty blob.
            rc = d.isEmpty() ? sqlite3_bind_zeroblob(stmt, i + 1, 0)
                             : sqlite3_bind_blob64(stmt, i + 1, d.constData(),
                                                   sqlite3_uint64(d.size()), SQLITE_TRANSIENT);
            break;
        }
        if (rc != SQLITE_OK)
            return rc;
    }
    return SQLITE_OK;
}

QString forEachRow(sqlite3 *db, const QByteArray &sql, const QStringList &parameters,
                   const std::function<void(sqlite3_stmt *)> &row)
{
    sqlite3_stmt *raw = nullptr;
    int rc = sqlite3_prepare_v2(db, sql.constData(), int(sql.size()), &raw, nullptr);
    StatementPtr stmt(raw);
    if (rc != SQLITE_OK)
        return QString::fromUtf8(sqlite3_errmsg(db));
    for (int i = 0; i < parameters.size(); ++i) {
        const QByteArray value = parameters[i].toUtf8();
        sqlite3_bind_text64(stmt.get(), i + 1, value.constData(), sqlite3_uint64(value.size()),
                            SQLITE_TRANSIENT, SQLITE_UTF8);
    }
    while ((rc = sqlite3_step(stmt.get())) == SQLITE_ROW) {
        if (row)
            row(stmt.get());
    }
    return rc == SQLITE_DONE ? QString() : QString::fromUtf8(sqlite3_errmsg(db));
}

QString exec(sqlite3 *db, const QByteArray &sql)
{
    char *error = nullptr;
    if (sqlite3_exec(db, sql.constData(), nullptr, nullptr, &error) == SQLITE_OK)
        return {};
    const QString message = QString::fromUtf8(error ? error : sqlite3_errmsg(db));
    sqlite3_free(error);
    return message;
}

QString columnText(sqlite3_stmt *stmt, int column)
{
    const auto *text = reinterpret_cast<const char *>(sqlite3_column_text(stmt, column));
    return text ? QString::fromUtf8(text, sqlite3_column_bytes(stmt, column)) : QString();
}

} // namespace sqliteman::sqlite
