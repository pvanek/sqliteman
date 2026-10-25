// SPDX-FileCopyrightText: 2026 Petr Vanek
// SPDX-License-Identifier: GPL-3.0-or-later

#include "sqlite/Quote.h"

namespace sqliteman::sqlite {

QString quoteIdentifier(const QString &name)
{
    QString quoted = name;
    quoted.replace(QLatin1Char('"'), QLatin1String("\"\""));
    return QLatin1Char('"') + quoted + QLatin1Char('"');
}

QString qualifiedName(const QString &schema, const QString &name)
{
    if (schema.isEmpty())
        return quoteIdentifier(name);
    return quoteIdentifier(schema) + QLatin1Char('.') + quoteIdentifier(name);
}

QString quoteLiteral(const QString &text)
{
    QString quoted = text;
    quoted.replace(QLatin1Char('\''), QLatin1String("''"));
    return QLatin1Char('\'') + quoted + QLatin1Char('\'');
}

QString blobLiteral(const QByteArray &bytes)
{
    return QLatin1String("X'") + QString::fromLatin1(bytes.toHex().toUpper()) + QLatin1Char('\'');
}

QString literal(const Value &value)
{
    switch (value.type()) {
    case ValueType::Null:
        return QStringLiteral("NULL");
    case ValueType::Integer:
        return value.toString();
    case ValueType::Real:
        // SQLite has no literal for infinity; 9e999 reads as one.
        if (value.data() == "Inf")
            return QStringLiteral("9e999");
        if (value.data() == "-Inf")
            return QStringLiteral("-9e999");
        return value.toString();
    case ValueType::Text:
        return quoteLiteral(value.toString());
    case ValueType::Blob:
        return blobLiteral(value.data());
    }
    return {};
}

} // namespace sqliteman::sqlite
