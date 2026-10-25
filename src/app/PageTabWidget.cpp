// SPDX-FileCopyrightText: 2026 Petr Vanek
// SPDX-License-Identifier: GPL-3.0-or-later

#include "PageTabWidget.h"

#include "WorkspacePage.h"

#include <QMenu>
#include <QTabBar>

namespace sqliteman {

PageTabWidget::PageTabWidget(QWidget *parent) : QTabWidget(parent)
{
    setDocumentMode(true);
    setTabsClosable(true);
    setMovable(true);
    tabBar()->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(tabBar(), &QTabBar::customContextMenuRequested, this, &PageTabWidget::showContextMenu);
}

void PageTabWidget::addPage(WorkspacePage *page)
{
    addTab(page, page->title());
    updatePage(page);
    setCurrentWidget(page);
}

void PageTabWidget::takePage(WorkspacePage *page)
{
    const int index = indexOf(page);
    if (index >= 0)
        removeTab(index);
    page->setParent(nullptr);
}

QList<WorkspacePage *> PageTabWidget::pages() const
{
    QList<WorkspacePage *> out;
    for (int i = 0; i < count(); ++i) {
        if (auto *page = qobject_cast<WorkspacePage *>(widget(i)))
            out << page;
    }
    return out;
}

WorkspacePage *PageTabWidget::currentPage() const
{
    return qobject_cast<WorkspacePage *>(currentWidget());
}

void PageTabWidget::updatePage(WorkspacePage *page)
{
    const int index = indexOf(page);
    if (index < 0)
        return;
    setTabText(index, page->title());
    setTabToolTip(index, page->toolTip());
    setTabIcon(index, page->icon());
}

void PageTabWidget::tabRemoved(int index)
{
    QTabWidget::tabRemoved(index);
    if (count() == 0)
        Q_EMIT emptied();
}

void PageTabWidget::showContextMenu(const QPoint &pos)
{
    auto *page = qobject_cast<WorkspacePage *>(widget(tabBar()->tabAt(pos)));
    if (!page)
        return;
    QMenu menu(this);
    menu.addAction(tr("Move to New Window"), this, [this, page] { Q_EMIT detachRequested(page); });
    if (!m_main)
        menu.addAction(tr("Move to Main Window"), this,
                       [this, page] { Q_EMIT moveToMainRequested(page); });
    menu.exec(tabBar()->mapToGlobal(pos));
}

} // namespace sqliteman
