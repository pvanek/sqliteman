// SPDX-FileCopyrightText: 2026 Petr Vanek
// SPDX-License-Identifier: GPL-3.0-or-later

#include "config/Preferences.h"
#include "config/RecentFiles.h"

#include <QFile>
#include <QSettings>
#include <QTemporaryDir>
#include <QTest>

using sqliteman::config::Preferences;
using sqliteman::config::RecentFiles;

class TestConfig : public QObject
{
    Q_OBJECT

private:
    QTemporaryDir m_dir;

    QString touch(const QString &name)
    {
        const QString path = m_dir.filePath(name);
        QFile f(path);
        if (!f.open(QIODevice::WriteOnly))
            qFatal("Cannot create %s", qPrintable(path));
        return path;
    }

private Q_SLOTS:
    void defaultsWithoutSettings()
    {
        QSettings settings(m_dir.filePath(QStringLiteral("empty.ini")), QSettings::IniFormat);
        Preferences p;
        p.load(settings);
        QCOMPARE(p.nullText, QStringLiteral("NULL"));
        QVERIFY(p.foreignKeys);
        QCOMPARE(p.rowLimit, 100'000);
        // A fresh install has templates to start from.
        QVERIFY(p.snippets.contains(QStringLiteral("sel")));
    }

    void roundTrip()
    {
        const QString file = m_dir.filePath(QStringLiteral("prefs.ini"));
        Preferences p;
        p.nullText = QStringLiteral("<null>");
        p.nullColor = QColor(Qt::yellow);
        p.rowLimit = 500;
        p.foreignKeys = false;
        p.snippets = {{QStringLiteral("x"), QStringLiteral("SELECT 1;")}};
        p.startupExtensions = {QStringLiteral("/usr/lib/mod_spatialite.so")};
        {
            QSettings settings(file, QSettings::IniFormat);
            p.save(settings);
        }
        QSettings settings(file, QSettings::IniFormat);
        Preferences q;
        q.load(settings);
        QCOMPARE(q.nullText, p.nullText);
        QCOMPARE(q.nullColor, p.nullColor);
        QCOMPARE(q.rowLimit, 500);
        QVERIFY(!q.foreignKeys);
        QCOMPARE(q.snippets, p.snippets);
        QCOMPARE(q.startupExtensions, p.startupExtensions);
        QVERIFY(!q.blobColor.isValid());
    }

    void deletingEverySnippetSticks()
    {
        // No templates at all is a choice, not a fresh install.
        const QString file = m_dir.filePath(QStringLiteral("nosnippets.ini"));
        {
            QSettings settings(file, QSettings::IniFormat);
            Preferences p;
            p.snippets.clear();
            p.save(settings);
        }
        QSettings settings(file, QSettings::IniFormat);
        Preferences q;
        q.load(settings);
        QVERIFY(q.snippets.isEmpty());
    }

    void recentFiles()
    {
        QSettings settings(m_dir.filePath(QStringLiteral("recent.ini")), QSettings::IniFormat);
        RecentFiles recent(settings, QStringLiteral("recent/databases"));
        const QString a = touch(QStringLiteral("a.db"));
        const QString b = touch(QStringLiteral("b.db"));
        const QString c = touch(QStringLiteral("c.db"));
        recent.add(a, 2);
        recent.add(b, 2);
        QCOMPARE(recent.list(), (QStringList {b, a}));
        recent.add(a, 2); // Again: to the front, not twice.
        QCOMPARE(recent.list(), (QStringList {a, b}));
        recent.add(c, 2); // The oldest goes.
        QCOMPARE(recent.list(), (QStringList {c, a}));
        QFile::remove(c); // A file that is gone is not offered.
        QCOMPARE(recent.list(), (QStringList {a}));
        recent.remove(a);
        QVERIFY(recent.list().isEmpty());
    }
};

QTEST_GUILESS_MAIN(TestConfig)
#include "tst_config.moc"
