// SPDX-FileCopyrightText: 2007-2026 Petr Vanek
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QStringList>

class QSettings;

namespace sqliteman::config {

// The databases opened lately, most recent first, in QSettings under one
// key, so File → Open Recent lists them in every window and session.
class RecentFiles
{
public:
    RecentFiles(QSettings &settings, const QString &key) : m_settings(settings), m_key(key) { }

    // The paths as absolute paths, those that are gone left out.
    QStringList list() const;
    // Puts a path first, keeping at most limit of them.
    void add(const QString &path, int limit);
    void remove(const QString &path);
    void clear();

private:
    QSettings &m_settings;
    QString m_key;
};

} // namespace sqliteman::config
