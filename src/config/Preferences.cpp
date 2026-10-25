// SPDX-FileCopyrightText: 2007-2026 Petr Vanek
// SPDX-License-Identifier: GPL-3.0-or-later

#include "config/Preferences.h"

#include <QSettings>

namespace sqliteman::config {

namespace {

QString key(const char *name)
{
    return QLatin1String("preferences/") + QLatin1String(name);
}

} // namespace

void Preferences::load(const QSettings &s)
{
    const Preferences d;
    recentCount = s.value(key("recentCount"), d.recentCount).toInt();
    reopenDatabases = s.value(key("reopenDatabases"), d.reopenDatabases).toBool();

    nullText = s.value(key("nullText"), d.nullText).toString();
    blobText = s.value(key("blobText"), d.blobText).toString();
    highlightNull = s.value(key("highlightNull"), d.highlightNull).toBool();
    highlightBlob = s.value(key("highlightBlob"), d.highlightBlob).toBool();
    nullColor = s.value(key("nullColor")).value<QColor>();
    blobColor = s.value(key("blobColor")).value<QColor>();
    rowLimit = s.value(key("rowLimit"), d.rowLimit).toInt();

    editorFont = s.value(key("editorFont")).toString();
    highlightCurrentLine = s.value(key("highlightCurrentLine"), d.highlightCurrentLine).toBool();
    widthMark = s.value(key("widthMark"), d.widthMark).toInt();
    completion = s.value(key("completion"), d.completion).toBool();
    completionChars = s.value(key("completionChars"), d.completionChars).toInt();
    useSnippets = s.value(key("useSnippets"), d.useSnippets).toBool();
    snippets.clear();
    if (s.contains(key("snippets"))) {
        const QVariantMap stored = s.value(key("snippets")).toMap();
        for (auto it = stored.cbegin(); it != stored.cend(); ++it)
            snippets.insert(it.key(), it.value().toString());
    } else {
        snippets = defaultSnippets();
    }

    foreignKeys = s.value(key("foreignKeys"), d.foreignKeys).toBool();
    busyTimeoutMs = s.value(key("busyTimeoutMs"), d.busyTimeoutMs).toInt();
    allowExtensions = s.value(key("allowExtensions"), d.allowExtensions).toBool();
    startupExtensions = s.value(key("startupExtensions")).toStringList();
}

void Preferences::save(QSettings &s) const
{
    s.setValue(key("recentCount"), recentCount);
    s.setValue(key("reopenDatabases"), reopenDatabases);

    s.setValue(key("nullText"), nullText);
    s.setValue(key("blobText"), blobText);
    s.setValue(key("highlightNull"), highlightNull);
    s.setValue(key("highlightBlob"), highlightBlob);
    s.setValue(key("nullColor"), nullColor);
    s.setValue(key("blobColor"), blobColor);
    s.setValue(key("rowLimit"), rowLimit);

    s.setValue(key("editorFont"), editorFont);
    s.setValue(key("highlightCurrentLine"), highlightCurrentLine);
    s.setValue(key("widthMark"), widthMark);
    s.setValue(key("completion"), completion);
    s.setValue(key("completionChars"), completionChars);
    s.setValue(key("useSnippets"), useSnippets);
    QVariantMap stored;
    for (auto it = snippets.cbegin(); it != snippets.cend(); ++it)
        stored.insert(it.key(), it.value());
    s.setValue(key("snippets"), stored);

    s.setValue(key("foreignKeys"), foreignKeys);
    s.setValue(key("busyTimeoutMs"), busyTimeoutMs);
    s.setValue(key("allowExtensions"), allowExtensions);
    s.setValue(key("startupExtensions"), startupExtensions);
}

QMap<QString, QString> Preferences::defaultSnippets()
{
    return {
        {QStringLiteral("sel"), QStringLiteral("SELECT * FROM ")},
        {QStringLiteral("selc"), QStringLiteral("SELECT count(*) FROM ")},
        {QStringLiteral("ins"), QStringLiteral("INSERT INTO  VALUES ();")},
        {QStringLiteral("upd"), QStringLiteral("UPDATE  SET  WHERE ;")},
        {QStringLiteral("del"), QStringLiteral("DELETE FROM  WHERE ;")},
        {QStringLiteral("ct"), QStringLiteral("CREATE TABLE  (\n    id INTEGER PRIMARY KEY\n);")},
    };
}

} // namespace sqliteman::config
