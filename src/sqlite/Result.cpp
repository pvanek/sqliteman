// SPDX-FileCopyrightText: 2026 Petr Vanek
// SPDX-License-Identifier: GPL-3.0-or-later

#include "sqlite/Result.h"

namespace sqliteman::sqlite {

void RowChunk::add(ValueType type, QByteArrayView bytes)
{
    if (offsets.empty())
        offsets.push_back(0);
    data.append(bytes.data(), bytes.size());
    offsets.push_back(std::uint32_t(data.size()));
    types.push_back(type);
}

QByteArrayView RowChunk::value(int row, int column) const
{
    const std::size_t cell = std::size_t(row) * std::size_t(columns) + std::size_t(column);
    const std::uint32_t from = offsets[cell];
    return QByteArrayView(data.constData() + from, qsizetype(offsets[cell + 1] - from));
}

const std::vector<Column> &Result::columns() const
{
    static const std::vector<Column> none;
    return m_data && m_data->columns ? *m_data->columns : none;
}

const RowChunk &Result::chunk() const
{
    static const RowChunk none;
    return m_data ? m_data->rows : none;
}

Value Result::valueAt(int row, int column) const
{
    const ValueType t = type(row, column);
    if (t == ValueType::Null)
        return {};
    return {t, value(row, column).toByteArray()};
}

Result Result::error(const QString &message, int code)
{
    auto data = std::make_shared<Data>();
    data->kind = Kind::Error;
    data->error = message;
    data->errorCode = code;
    return Result(std::move(data));
}

} // namespace sqliteman::sqlite
