// SPDX-FileCopyrightText: 2026 Petr Vanek
// SPDX-License-Identifier: GPL-3.0-or-later

#include "sql/Keywords.h"

#include <QStringList>

#include <sqlite3.h>

#include <algorithm>

namespace sqliteman::sql {

namespace {

const std::vector<QByteArray> &keywordList()
{
    static const std::vector<QByteArray> list = [] {
        std::vector<QByteArray> words;
        const int count = sqlite3_keyword_count();
        words.reserve(std::size_t(count));
        for (int i = 0; i < count; ++i) {
            const char *name = nullptr;
            int length = 0;
            if (sqlite3_keyword_name(i, &name, &length) == SQLITE_OK)
                words.push_back(QByteArray(name, length).toUpper());
        }
        std::sort(words.begin(), words.end());
        return words;
    }();
    return list;
}

} // namespace

bool isKeyword(QByteArrayView word)
{
    if (word.isEmpty() || word.size() > 20)
        return false;
    return sqlite3_keyword_check(word.data(), int(word.size())) != 0;
}

std::vector<QByteArray> keywords()
{
    return keywordList();
}

QStringList typeNames()
{
    return {QStringLiteral("INTEGER"),  QStringLiteral("TEXT"),    QStringLiteral("REAL"),
            QStringLiteral("BLOB"),     QStringLiteral("NUMERIC"), QStringLiteral("ANY"),
            QStringLiteral("INT"),      QStringLiteral("BOOLEAN"), QStringLiteral("DATE"),
            QStringLiteral("DATETIME"), QStringLiteral("VARCHAR"), QStringLiteral("DOUBLE"),
            QStringLiteral("DECIMAL")};
}

bool needsQuoting(QByteArrayView identifier)
{
    if (identifier.isEmpty())
        return true;
    const auto plain = [](char c, bool first) {
        return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_'
            || (!first && c >= '0' && c <= '9');
    };
    for (qsizetype i = 0; i < identifier.size(); ++i) {
        if (!plain(identifier[i], i == 0))
            return true;
    }
    return isKeyword(identifier);
}

QString identifier(const QString &name)
{
    if (!needsQuoting(name.toUtf8()))
        return name;
    QString quoted = name;
    quoted.replace(QLatin1Char('"'), QLatin1String("\"\""));
    return QLatin1Char('"') + quoted + QLatin1Char('"');
}

} // namespace sqliteman::sql
