// SPDX-FileCopyrightText: 2026 Petr Vanek
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "catalog/Plan.h"

#include <QWidget>

class QAction;
class QHBoxLayout;
class QLabel;
class QStackedWidget;
class QToolButton;
class QTreeWidget;

namespace sqliteman {

// A query plan as a tree, as EXPLAIN QUERY PLAN describes it: full scans
// of a table marked, as they are what usually makes a query slow.
class PlanView : public QWidget
{
    Q_OBJECT

public:
    explicit PlanView(QWidget *parent = nullptr);

    void setPlan(const catalog::Plan &plan, const QString &title);
    void showMessage(const QString &text, bool error = false);
    void clear();
    bool hasPlan() const { return !m_plan.isEmpty(); }
    const catalog::Plan &plan() const { return m_plan; }
    QString title() const;
    QTreeWidget *tree() const { return m_tree; }
    // A button in the header, for what whoever holds the view adds to it.
    QToolButton *addHeaderAction(QAction *action);

Q_SIGNALS:
    void changed();

private:
    QHBoxLayout *m_header = nullptr;
    QLabel *m_title = nullptr;
    QStackedWidget *m_stack = nullptr;
    QTreeWidget *m_tree = nullptr;
    QLabel *m_message = nullptr;
    catalog::Plan m_plan;
};

} // namespace sqliteman
