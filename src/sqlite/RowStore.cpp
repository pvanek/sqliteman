// SPDX-FileCopyrightText: 2026 Petr Vanek
// SPDX-License-Identifier: GPL-3.0-or-later

#include "sqlite/RowStore.h"

#include <algorithm>

namespace sqliteman::sqlite {

void RowStore::clear()
{
    m_columns.reset();
    m_chunks.clear();
    m_firstRow.clear();
    m_rows = 0;
}

void RowStore::append(const Result &result)
{
    if (result.isNull() || result.columnCount() == 0)
        return;
    if (!m_columns)
        m_columns = result.sharedColumns();
    if (result.rowCount() == 0 || result.columnCount() != columnCount())
        return;
    m_firstRow.push_back(m_rows);
    m_rows += result.rowCount();
    m_chunks.push_back(result);
}

const std::vector<Column> &RowStore::columns() const
{
    static const std::vector<Column> none;
    return m_columns ? *m_columns : none;
}

std::pair<const RowChunk *, int> RowStore::locate(int row) const
{
    const auto it = std::upper_bound(m_firstRow.begin(), m_firstRow.end(), row);
    const auto index = std::size_t(it - m_firstRow.begin()) - 1;
    return {&m_chunks[index].chunk(), row - m_firstRow[index]};
}

ValueType RowStore::type(int row, int column) const
{
    const auto [chunk, r] = locate(row);
    return chunk->type(r, column);
}

QByteArrayView RowStore::value(int row, int column) const
{
    const auto [chunk, r] = locate(row);
    return chunk->value(r, column);
}

Value RowStore::valueAt(int row, int column) const
{
    const auto [chunk, r] = locate(row);
    const ValueType t = chunk->type(r, column);
    if (t == ValueType::Null)
        return {};
    return {t, chunk->value(r, column).toByteArray()};
}

qsizetype RowStore::memoryUsed() const
{
    qsizetype bytes = 0;
    for (const Result &r : m_chunks) {
        const RowChunk &c = r.chunk();
        bytes += c.data.capacity() + qsizetype(c.offsets.capacity() * sizeof(std::uint32_t))
            + qsizetype(c.types.capacity());
    }
    return bytes;
}

} // namespace sqliteman::sqlite
