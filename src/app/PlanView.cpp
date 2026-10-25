// SPDX-FileCopyrightText: 2026 Petr Vanek
// SPDX-License-Identifier: GPL-3.0-or-later

#include "PlanView.h"

#include <QAction>
#include <QApplication>
#include <QClipboard>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QPalette>
#include <QStackedWidget>
#include <QToolButton>
#include <QTreeWidget>
#include <QVBoxLayout>

namespace sqliteman {

using catalog::PlanStep;

PlanView::PlanView(QWidget *parent)
    : QWidget(parent), m_title(new QLabel(this)), m_stack(new QStackedWidget(this)),
      m_tree(new QTreeWidget(this)), m_message(new QLabel(this))
{
    QFont bold = m_title->font();
    bold.setBold(true);
    m_title->setFont(bold);
    m_title->setTextFormat(Qt::PlainText);

    m_tree->setHeaderLabels({tr("Step"), tr("Reads")});
    m_tree->header()->setStretchLastSection(false);
    m_tree->header()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_tree->header()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    m_tree->setUniformRowHeights(true);

    auto *copy = new QToolButton(this);
    copy->setIcon(QIcon::fromTheme(QStringLiteral("edit-copy")));
    copy->setText(tr("Copy"));
    copy->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    copy->setToolTip(tr("Copy the plan as text, as the sqlite3 shell prints it"));
    copy->setAutoRaise(true);
    connect(copy, &QToolButton::clicked, this, [this] {
        if (hasPlan())
            QGuiApplication::clipboard()->setText(catalog::planText(m_plan));
    });

    m_message->setWordWrap(true);
    m_message->setAlignment(Qt::AlignTop | Qt::AlignLeft);
    m_message->setTextInteractionFlags(Qt::TextSelectableByMouse);
    m_message->setMargin(8);
    m_stack->addWidget(m_message);
    m_stack->addWidget(m_tree);

    m_header = new QHBoxLayout;
    m_header->setContentsMargins(4, 2, 4, 2);
    m_header->addWidget(m_title);
    m_header->addStretch();
    m_header->addWidget(copy);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    layout->addLayout(m_header);
    layout->addWidget(m_stack, 1);

    showMessage(tr("Explain a statement with Ctrl+E to see how SQLite runs it."));
}

QString PlanView::title() const
{
    return m_title->text();
}

QToolButton *PlanView::addHeaderAction(QAction *action)
{
    auto *button = new QToolButton(this);
    button->setDefaultAction(action);
    button->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    button->setAutoRaise(true);
    m_header->insertWidget(m_header->count() - 1, button);
    return button;
}

void PlanView::setPlan(const catalog::Plan &plan, const QString &title)
{
    m_plan = plan;
    m_title->setText(title);
    m_tree->clear();
    const bool dark = QGuiApplication::palette().color(QPalette::Base).lightness() < 128;
    const QColor warning = dark ? QColor(0xf2, 0xb8, 0x4b) : QColor(0xb3, 0x59, 0x00);

    std::function<void(QTreeWidgetItem *, int)> add = [&](QTreeWidgetItem *parent, int index) {
        const PlanStep &step = m_plan.steps[std::size_t(index)];
        auto *item = parent ? new QTreeWidgetItem(parent) : new QTreeWidgetItem(m_tree);
        item->setText(0, step.detail);
        item->setText(1, step.object);
        switch (step.kind) {
        case PlanStep::Kind::Scan:
            item->setForeground(0, warning);
            item->setToolTip(0, tr("Reads every row of %1").arg(step.object));
            break;
        case PlanStep::Kind::TempBTree:
            item->setForeground(0, warning);
            item->setToolTip(0, tr("Sorts or collects rows in a temporary table"));
            break;
        case PlanStep::Kind::Search:
            item->setToolTip(0, tr("Finds rows through %1").arg(step.object));
            break;
        case PlanStep::Kind::Other:
            break;
        }
        for (const int child : step.children)
            add(item, child);
    };
    for (const int root : m_plan.roots)
        add(nullptr, root);
    m_tree->expandAll();
    m_stack->setCurrentWidget(m_tree);
    Q_EMIT changed();
}

void PlanView::showMessage(const QString &text, bool error)
{
    m_plan = {};
    m_tree->clear();
    m_message->setText(text);
    m_message->setStyleSheet(error ? QStringLiteral("color: #c62828;") : QString());
    m_stack->setCurrentWidget(m_message);
    Q_EMIT changed();
}

void PlanView::clear()
{
    showMessage(QString());
    m_title->clear();
}

} // namespace sqliteman
