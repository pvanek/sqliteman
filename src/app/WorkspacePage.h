// SPDX-FileCopyrightText: 2026 Petr Vanek
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QIcon>
#include <QWidget>

namespace sqliteman {

// A page of the main window's work area, one per tab: an SQL editor with
// its results, a table's rows, or an object's details. The main window
// knows pages only through this interface.
class WorkspacePage : public QWidget
{
    Q_OBJECT

public:
    using QWidget::QWidget;

    // The tab's text.
    virtual QString title() const = 0;
    // The tab's icon: what kind of page it is.
    virtual QIcon icon() const { return {}; }
    // Before the page closes: it may ask to save its work. False keeps it open.
    virtual bool maybeClose() { return true; }

Q_SIGNALS:
    // The title, icon or tooltip changed.
    void titleChanged();
};

} // namespace sqliteman
