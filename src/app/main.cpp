// SPDX-FileCopyrightText: 2007-2026 Petr Vanek
// SPDX-License-Identifier: GPL-3.0-or-later

#include "MainWindow.h"

#include <QApplication>
#include <QCommandLineParser>
#include <QIcon>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    QApplication::setApplicationName(QStringLiteral("sqliteman"));
    QApplication::setApplicationDisplayName(QStringLiteral("Sqliteman"));
    QApplication::setApplicationVersion(QStringLiteral(SQLITEMAN_VERSION));
    QApplication::setOrganizationName(QStringLiteral("yarpen.cz"));
    QApplication::setOrganizationDomain(QStringLiteral("yarpen.cz"));
    // Lets Wayland shells match windows with cz.yarpen.sqliteman.desktop and
    // its icon.
    QGuiApplication::setDesktopFileName(QStringLiteral("cz.yarpen.sqliteman"));

    QIcon icon;
    for (const int size : {16, 22, 24, 32, 48, 64, 128, 256, 512})
        icon.addFile(QStringLiteral(":/icons/sqliteman-%1.png").arg(size), QSize(size, size));
    QApplication::setWindowIcon(icon);

    QCommandLineParser parser;
    parser.setApplicationDescription(
        QApplication::translate("main", "SQLite databases made easy."));
    parser.addHelpOption();
    parser.addVersionOption();
    parser.addPositionalArgument(
        QStringLiteral("files"),
        QApplication::translate("main", "Databases to open, and SQL scripts to open in editors."),
        QStringLiteral("[files...]"));
    parser.process(app);

    sqliteman::MainWindow window;
    window.show();
    for (const QString &path : parser.positionalArguments())
        window.open(path);
    return QApplication::exec();
}
