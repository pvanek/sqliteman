// SPDX-FileCopyrightText: 2026 Petr Vanek
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "sqlite/Result.h"

#include <QByteArrayView>

#include <vector>

namespace sqliteman::sqlite {

// The rows of a query, which may arrive in many results, one chunk each.
// The results are kept as they came, shared rather than copied.
class RowStore
{
public:
    void clear();
    // Adds a result's rows. The first result with columns sets them; later
    // ones are chunks of the same statement.
    void append(const Result &result);

    int rowCount() const { return m_rows; }
    int columnCount() const { return int(columns().size()); }
    bool hasColumns() const { return m_columns != nullptr; }
    const std::vector<Column> &columns() const;
    const Column &column(int c) const { return columns()[std::size_t(c)]; }

    ValueType type(int row, int column) const;
    bool isNull(int row, int column) const { return type(row, column) == ValueType::Null; }
    QByteArrayView value(int row, int column) const;
    Value valueAt(int row, int column) const;

    // Bytes held, including spare capacity.
    qsizetype memoryUsed() const;

private:
    std::pair<const RowChunk *, int> locate(int row) const;

    std::shared_ptr<const std::vector<Column>> m_columns;
    std::vector<Result> m_chunks; // Only those with rows.
    std::vector<int> m_firstRow; // Of each chunk.
    int m_rows = 0;
};

} // namespace sqliteman::sqlite
