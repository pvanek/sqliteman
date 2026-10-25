// SPDX-FileCopyrightText: 2026 Petr Vanek
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "sqlite/Value.h"

#include <QByteArray>
#include <QByteArrayView>
#include <QMetaType>
#include <QString>

#include <cstdint>
#include <memory>
#include <vector>

namespace sqliteman::sqlite {

// A result column and, when it is read straight from a table, where from.
struct Column
{
    QString name;
    QString declaredType; // As in CREATE TABLE; empty for an expression.
    QString database; // "main", "temp" or an attached database's name.
    QString table;
    QString origin; // The column's name in that table.

    bool operator==(const Column &) const = default;
};

// Rows as they come out of SQLite, copied into one byte buffer with an
// offset table: cheap to pass between threads and to keep many of.
struct RowChunk
{
    QByteArray data;
    std::vector<std::uint32_t> offsets; // rows * columns + 1: where each cell starts.
    std::vector<ValueType> types;
    int rows = 0;
    int columns = 0;

    void add(ValueType type, QByteArrayView bytes);
    QByteArrayView value(int row, int column) const;
    ValueType type(int row, int column) const
    {
        return types[std::size_t(row) * std::size_t(columns) + std::size_t(column)];
    }
};

// What one statement of a query gave: some of its rows while more are on
// the way, or how it ended. Shared and immutable, so cheap to copy and to
// pass through signals.
class Result
{
public:
    enum class Kind : std::uint8_t {
        Rows, // A chunk of rows; more chunks or the statement's end follow.
        Done, // The statement finished; its last rows, if any, come with it.
        Error, // The statement failed and the query stopped there.
    };

    struct Data
    {
        Kind kind = Kind::Done;
        std::shared_ptr<const std::vector<Column>> columns;
        RowChunk rows;
        // The statement within the SQL sent, in bytes.
        qsizetype offset = 0;
        qsizetype length = 0;
        int changes = 0; // Rows the statement inserted, updated or deleted.
        qint64 lastInsertRowid = 0;
        bool readOnly = true; // sqlite3_stmt_readonly(): changes nothing.
        bool changesSchema = false; // DDL, ATTACH or DETACH.
        bool truncated = false; // Stopped at the row limit, more rows were there.
        QString error;
        int errorCode = 0; // Extended result code.
        qsizetype errorOffset = -1; // In bytes within the SQL sent; -1 if not known.
        int binding = -1; // With several sets of parameters: the set that failed.
    };

    Result() = default;
    explicit Result(std::shared_ptr<const Data> data) : m_data(std::move(data)) { }

    bool isNull() const { return !m_data; }
    Kind kind() const { return m_data ? m_data->kind : Kind::Error; }
    bool isError() const { return kind() == Kind::Error; }
    bool isChunk() const { return kind() == Kind::Rows; }

    int columnCount() const { return m_data && m_data->columns ? int(m_data->columns->size()) : 0; }
    const std::vector<Column> &columns() const;
    std::shared_ptr<const std::vector<Column>> sharedColumns() const
    {
        return m_data ? m_data->columns : nullptr;
    }
    const Column &column(int c) const { return columns()[std::size_t(c)]; }
    const RowChunk &chunk() const;
    int rowCount() const { return m_data ? m_data->rows.rows : 0; }
    ValueType type(int row, int column) const { return chunk().type(row, column); }
    bool isNull(int row, int column) const { return type(row, column) == ValueType::Null; }
    QByteArrayView value(int row, int column) const { return chunk().value(row, column); }
    Value valueAt(int row, int column) const;

    qsizetype offset() const { return m_data ? m_data->offset : 0; }
    qsizetype length() const { return m_data ? m_data->length : 0; }
    int changes() const { return m_data ? m_data->changes : 0; }
    qint64 lastInsertRowid() const { return m_data ? m_data->lastInsertRowid : 0; }
    bool isReadOnly() const { return !m_data || m_data->readOnly; }
    bool changesSchema() const { return m_data && m_data->changesSchema; }
    bool isTruncated() const { return m_data && m_data->truncated; }

    QString errorMessage() const { return m_data ? m_data->error : QString(); }
    int errorCode() const { return m_data ? m_data->errorCode : 0; }
    qsizetype errorOffset() const { return m_data ? m_data->errorOffset : -1; }
    int failedBinding() const { return m_data ? m_data->binding : -1; }

    static Result error(const QString &message, int code = 1);

private:
    std::shared_ptr<const Data> m_data;
};

} // namespace sqliteman::sqlite

Q_DECLARE_METATYPE(sqliteman::sqlite::Result)
