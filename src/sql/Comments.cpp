// SPDX-FileCopyrightText: 2026 Petr Vanek
// SPDX-License-Identifier: GPL-3.0-or-later

#include "sql/Comments.h"

#include <QList>

#include <algorithm>
#include <limits>

namespace sqliteman::sql {

namespace {

qsizetype indentation(const QByteArray &line)
{
    qsizetype i = 0;
    while (i < line.size() && (line[i] == ' ' || line[i] == '\t'))
        ++i;
    return i;
}

// The \r of a \r\n line end is not content either.
bool isBlank(const QByteArray &line)
{
    const qsizetype i = indentation(line);
    return i == line.size() || (i == line.size() - 1 && line[i] == '\r');
}

} // namespace

QByteArray toggleLineComments(const QByteArray &lines)
{
    QList<QByteArray> split = lines.split('\n');
    bool commented = true;
    bool any = false;
    qsizetype indent = std::numeric_limits<qsizetype>::max();
    for (const QByteArray &line : split) {
        if (isBlank(line))
            continue;
        any = true;
        const qsizetype i = indentation(line);
        indent = std::min(indent, i);
        if (!line.sliced(i).startsWith("--"))
            commented = false;
    }
    if (!any)
        return lines;

    for (QByteArray &line : split) {
        if (isBlank(line))
            continue;
        if (commented) {
            const qsizetype at = indentation(line);
            line.remove(at, at + 2 < line.size() && line[at + 2] == ' ' ? 3 : 2);
        } else {
            line.insert(indent, "-- ");
        }
    }
    return split.join('\n');
}

} // namespace sqliteman::sql
