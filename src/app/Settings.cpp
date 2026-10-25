// SPDX-FileCopyrightText: 2026 Petr Vanek
// SPDX-License-Identifier: GPL-3.0-or-later

#include "Settings.h"

#include <QFontDatabase>

namespace sqliteman {

Settings &Settings::instance()
{
    static Settings settings;
    return settings;
}

Settings::Settings()
{
    m_preferences.load(m_store);
}

void Settings::setPreferences(const config::Preferences &preferences)
{
    m_preferences = preferences;
    m_preferences.save(m_store);
    Q_EMIT preferencesChanged();
}

QFont Settings::editorFont() const
{
    QFont font;
    if (m_preferences.editorFont.isEmpty() || !font.fromString(m_preferences.editorFont))
        return QFontDatabase::systemFont(QFontDatabase::FixedFont);
    return font;
}

} // namespace sqliteman
