// SPDX-FileCopyrightText: 2026 Petr Vanek
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QWidget>

class QHelpEngine;
class QLineEdit;
class QTabWidget;
class QTextBrowser;

namespace sqliteman {

class HelpBrowser;

// The manual, as built into sqliteman.qch: contents, index and search on the
// left, the page on the right. One window for the whole program.
class HelpWindow : public QWidget
{
    Q_OBJECT

public:
    ~HelpWindow() override;

    // Shows the window, on the page a keyword names, or on the first page
    // when the keyword is empty or unknown. Returns false when there is no
    // help file to show, so the caller can open the website instead.
    static bool show(const QString &keyword = {});
    // Where the help file is, or an empty string when there is none.
    static QString helpFilePath();

private:
    explicit HelpWindow(const QString &collectionFile);
    bool registerDocumentation(const QString &qchFile);
    void openKeyword(const QString &keyword);
    bool openPage(const QString &name);
    void openUrl(const QUrl &url);

    QHelpEngine *m_engine = nullptr;
    QTabWidget *m_navigation = nullptr;
    QLineEdit *m_search = nullptr;
    HelpBrowser *m_browser = nullptr;
};

} // namespace sqliteman
