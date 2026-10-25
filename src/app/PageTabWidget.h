// SPDX-FileCopyrightText: 2026 Petr Vanek
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QTabWidget>

namespace sqliteman {

class WorkspacePage;

// Tabs of workspace pages. A tab's context menu moves its page to a window
// of its own, and from there back to the main window.
class PageTabWidget : public QTabWidget
{
    Q_OBJECT

public:
    explicit PageTabWidget(QWidget *parent = nullptr);

    void addPage(WorkspacePage *page);
    // Removes a page without deleting it, to put it elsewhere.
    void takePage(WorkspacePage *page);
    QList<WorkspacePage *> pages() const;
    WorkspacePage *currentPage() const;
    // Shows a page's current title, icon and tooltip on its tab.
    void updatePage(WorkspacePage *page);

    // Whether this is the main window's; the tabs of every other window offer
    // "Move to Main Window".
    void setMain(bool main) { m_main = main; }
    bool isMain() const { return m_main; }

Q_SIGNALS:
    void detachRequested(sqliteman::WorkspacePage *page);
    void moveToMainRequested(sqliteman::WorkspacePage *page);
    // The last page left, by moving or closing.
    void emptied();

protected:
    void tabRemoved(int index) override;

private:
    void showContextMenu(const QPoint &pos);
    bool m_main = false;
};

} // namespace sqliteman
