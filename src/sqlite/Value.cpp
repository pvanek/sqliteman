// SPDX-FileCopyrightText: 2026 Petr Vanek
// SPDX-License-Identifier: GPL-3.0-or-later

#include "sqlite/Value.h"

#include <QLocale>

#include <cmath>

namespace sqliteman::sqlite {

Value Value::real(double v)
{
    // SQLite has no text for these; its shell prints Inf and NaN as NULL.
    if (std::isnan(v))
        return {};
    if (std::isinf(v))
        return {ValueType::Real, v > 0 ? QByteArrayLiteral("Inf") : QByteArrayLiteral("-Inf")};
    QByteArray text = QByteArray::number(v, 'g', QLocale::FloatingPointShortest);
    // A whole number written as such would read back as an integer.
    if (!text.contains('.') && !text.contains('e') && !text.contains('n'))
        text += ".0";
    return {ValueType::Real, text};
}

Value Value::fromUserText(const QString &v)
{
    const QString trimmed = v.trimmed();
    if (trimmed.size() == v.size() && !v.isEmpty()) {
        bool ok = false;
        const qint64 i = v.toLongLong(&ok);
        // Only when it is written the way SQLite would write it back, so that
        // "007" or "+1" stay the text they were typed as.
        if (ok && QString::number(i) == v)
            return integer(i);
        const double d = v.toDouble(&ok);
        if (ok && std::isfinite(d) && v.contains(QLatin1Char('.')))
            return real(d);
    }
    return text(v);
}

QString typeName(ValueType type)
{
    switch (type) {
    case ValueType::Null:
        return QStringLiteral("null");
    case ValueType::Integer:
        return QStringLiteral("integer");
    case ValueType::Real:
        return QStringLiteral("real");
    case ValueType::Text:
        return QStringLiteral("text");
    case ValueType::Blob:
        return QStringLiteral("blob");
    }
    return {};
}

} // namespace sqliteman::sqlite
