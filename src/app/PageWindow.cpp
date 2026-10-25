// SPDX-FileCopyrightText: 2026 Petr Vanek
// SPDX-License-Identifier: GPL-3.0-or-later

#include "PageWindow.h"

#include "PageTabWidget.h"
#include "WorkspacePage.h"

#include <QCloseEvent>
#include <QVBoxLayout>

namespace sqliteman {

PageWindow::PageWindow(QWidget *parent)
    : QWidget(parent, Qt::Window), m_tabs(new PageTabWidget(this))
{
    setAttribute(Qt::WA_DeleteOnClose);
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(m_tabs);
    connect(m_tabs, &QTabWidget::currentChanged, this, &PageWindow::updateTitle);
}

void PageWindow::updateTitle()
{
    const WorkspacePage *page = m_tabs->currentPage();
    setWindowTitle(page ? page->title() : QString());
}

void PageWindow::closeEvent(QCloseEvent *event)
{
    // The pages close with the window, if they agree (editors may ask to save).
    for (WorkspacePage *page : m_tabs->pages()) {
        m_tabs->setCurrentWidget(page);
        if (!page->maybeClose()) {
            event->ignore();
            return;
        }
    }
    event->accept();
}

} // namespace sqliteman
