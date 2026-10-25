// SPDX-FileCopyrightText: 2007-2026 Petr Vanek
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QColor>
#include <QMap>
#include <QString>
#include <QStringList>

class QSettings;

namespace sqliteman::config {

// What the Preferences dialog sets, kept in QSettings under "preferences/".
struct Preferences
{
    // General.
    int recentCount = 10; // Databases in File → Open Recent.
    bool reopenDatabases = false; // Open the databases that were open at the last exit.

    // Results.
    QString nullText = QStringLiteral("NULL");
    QString blobText = QStringLiteral("BLOB"); // Followed by the size.
    bool highlightNull = true; // In a background of its own.
    bool highlightBlob = true;
    QColor nullColor; // Invalid: a shade of the palette's.
    QColor blobColor;
    int rowLimit = 100'000; // A query stops reading after this many rows.

    // SQL editor.
    QString editorFont; // As QFont::toString() writes it; empty: the system's fixed font.
    bool highlightCurrentLine = true;
    int widthMark = 0; // A line at this column; 0 for none.
    bool completion = true; // The list pops up by itself while typing.
    int completionChars = 3; // Letters to type before it does.
    // Templates: a word typed and Tab replace the word with the text.
    bool useSnippets = true;
    QMap<QString, QString> snippets;

    // Database.
    bool foreignKeys = true; // PRAGMA foreign_keys on each database opened.
    int busyTimeoutMs = 5000;
    bool allowExtensions = false;
    QStringList startupExtensions; // Loaded into each database opened.

    void load(const QSettings &settings);
    void save(QSettings &settings) const;

    // The templates a fresh install starts with.
    static QMap<QString, QString> defaultSnippets();
};

} // namespace sqliteman::config
