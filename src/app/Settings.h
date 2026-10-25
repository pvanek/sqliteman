// SPDX-FileCopyrightText: 2026 Petr Vanek
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "config/Preferences.h"

#include <QObject>
#include <QSettings>

namespace sqliteman {

// The preferences the whole program goes by, and the settings file they
// and everything else are kept in. Editors and result views follow
// preferencesChanged().
class Settings : public QObject
{
    Q_OBJECT

public:
    static Settings &instance();

    QSettings &store() { return m_store; }
    const config::Preferences &preferences() const { return m_preferences; }
    // Saves them, and tells everyone.
    void setPreferences(const config::Preferences &preferences);

    // The editor font of the preferences, or the system's fixed font.
    QFont editorFont() const;

Q_SIGNALS:
    void preferencesChanged();

private:
    Settings();

    QSettings m_store;
    config::Preferences m_preferences;
};

} // namespace sqliteman
