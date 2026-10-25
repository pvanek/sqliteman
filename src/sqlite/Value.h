// SPDX-FileCopyrightText: 2026 Petr Vanek
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QByteArray>
#include <QMetaType>
#include <QString>

#include <cstdint>

namespace sqliteman::sqlite {

// SQLite's storage classes: every value has one of them, whatever the
// column was declared as.
enum class ValueType : std::uint8_t { Null, Integer, Real, Text, Blob };

// One value as SQLite stores it. Integers and reals are kept as their
// shortest decimal text that reads back as the same number, text as UTF-8
// and a blob as its bytes; so a value can be shown, compared and bound again
// without losing anything.
class Value
{
public:
    Value() = default;
    Value(ValueType type, QByteArray data) : m_type(type), m_data(std::move(data)) { }

    static Value null() { return {}; }
    static Value integer(qint64 v) { return {ValueType::Integer, QByteArray::number(v)}; }
    static Value real(double v);
    static Value text(const QString &v) { return {ValueType::Text, v.toUtf8()}; }
    static Value blob(const QByteArray &v) { return {ValueType::Blob, v}; }
    // A value typed into a cell: what SQLite's own type affinity would make of
    // it is not known here, so it stays text unless it reads as a number.
    static Value fromUserText(const QString &v);

    ValueType type() const { return m_type; }
    bool isNull() const { return m_type == ValueType::Null; }
    bool isNumeric() const { return m_type == ValueType::Integer || m_type == ValueType::Real; }
    // Text and numbers as UTF-8, a blob's bytes as they are.
    const QByteArray &data() const { return m_data; }

    qint64 toInteger() const { return m_data.toLongLong(); }
    double toReal() const { return m_data.toDouble(); }
    // Text and numbers as they read; a blob decoded as UTF-8; NULL empty.
    QString toString() const { return QString::fromUtf8(m_data); }

    bool operator==(const Value &other) const = default;

private:
    ValueType m_type = ValueType::Null;
    QByteArray m_data;
};

// The storage class's name, as typeof() returns it: "null", "integer" and so on.
QString typeName(ValueType type);

} // namespace sqliteman::sqlite

Q_DECLARE_METATYPE(sqliteman::sqlite::Value)
