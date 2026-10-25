// SPDX-FileCopyrightText: 2007-2026 Petr Vanek
// SPDX-License-Identifier: GPL-3.0-or-later

#include "config/RecentFiles.h"

#include <QFileInfo>
#include <QSettings>

namespace sqliteman::config {

QStringList RecentFiles::list() const
{
    QStringList out;
    for (const QString &path : m_settings.value(m_key).toStringList()) {
        if (QFileInfo::exists(path))
            out << path;
    }
    return out;
}

void RecentFiles::add(const QString &path, int limit)
{
    const QString absolute = QFileInfo(path).absoluteFilePath();
    QStringList paths = m_settings.value(m_key).toStringList();
    paths.removeAll(absolute);
    paths.prepend(absolute);
    while (paths.size() > std::max(limit, 0))
        paths.removeLast();
    m_settings.setValue(m_key, paths);
}

void RecentFiles::remove(const QString &path)
{
    QStringList paths = m_settings.value(m_key).toStringList();
    paths.removeAll(QFileInfo(path).absoluteFilePath());
    m_settings.setValue(m_key, paths);
}

void RecentFiles::clear()
{
    m_settings.remove(m_key);
}

} // namespace sqliteman::config
