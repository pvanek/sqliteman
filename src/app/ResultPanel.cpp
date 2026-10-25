// SPDX-FileCopyrightText: 2026 Petr Vanek
// SPDX-License-Identifier: GPL-3.0-or-later

#include "ResultPanel.h"

#include "PlanView.h"
#include "ResultView.h"

#include <QAction>
#include <QFontDatabase>
#include <QKeyEvent>
#include <QLineEdit>
#include <QMenu>
#include <QPlainTextEdit>
#include <QPointer>
#include <QTabBar>
#include <QTime>
#include <QToolButton>

namespace sqliteman {

namespace {

// Edits a tab's name over the tab. Esc, or the tab going, leaves the name
// as it was; Enter, or clicking elsewhere, keeps the new one.
class TabNameEdit : public QLineEdit
{
    Q_OBJECT

public:
    using QLineEdit::QLineEdit;

Q_SIGNALS:
    void cancelled();

protected:
    void keyPressEvent(QKeyEvent *event) override
    {
        if (event->key() == Qt::Key_Escape) {
            Q_EMIT cancelled();
            return;
        }
        QLineEdit::keyPressEvent(event);
    }
};

} // namespace

ResultPanel::ResultPanel(QWidget *parent) : QTabWidget(parent), m_messages(new QPlainTextEdit(this))
{
    setDocumentMode(true);
    setTabPosition(QTabWidget::South);
    setTabsClosable(true);
    m_messages->setReadOnly(true);
    m_messages->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
    m_messages->setMaximumBlockCount(5000);

    const QIcon pinIcon = QIcon::fromTheme(QStringLiteral("window-pin"));
    m_pin = new QAction(pinIcon, tr("Pin"), this);
    m_pin->setToolTip(
        tr("Keep these rows in a tab of their own; the next statement's go to a new one"));
    m_pin->setEnabled(false);
    connect(m_pin, &QAction::triggered, this, &ResultPanel::pin);
    m_pinPlan = new QAction(pinIcon, tr("Pin"), this);
    m_pinPlan->setToolTip(tr("Keep this plan in a tab of its own; the next one goes to a new one"));
    m_pinPlan->setEnabled(false);
    connect(m_pinPlan, &QAction::triggered, this, &ResultPanel::pinPlan);

    newResults();
    newPlan();
    addTab(m_messages, tr("Messages"));
    tabBar()->setTabButton(indexOf(m_messages), QTabBar::LeftSide, nullptr);
    tabBar()->setTabButton(indexOf(m_messages), QTabBar::RightSide, nullptr);
    connect(this, &QTabWidget::tabCloseRequested, this,
            [this](int index) { closePinned(widget(index)); });
    tabBar()->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(tabBar(), &QWidget::customContextMenuRequested, this, &ResultPanel::showTabMenu);
}

// Only pinned tabs close; Results, Plan and Messages are always there.
static void removeCloseButton(QTabBar *tabs, int index)
{
    tabs->setTabButton(index, QTabBar::LeftSide, nullptr);
    tabs->setTabButton(index, QTabBar::RightSide, nullptr);
}

void ResultPanel::newResults()
{
    m_results = new ResultView(this);
    insertTab(0, m_results, tr("Results"));
    m_pinButton = m_results->addHeaderAction(m_pin);
    removeCloseButton(tabBar(), 0);
}

void ResultPanel::newPlan()
{
    // After the pinned results, before the pinned plans.
    int index = indexOf(m_results) + 1;
    while (qobject_cast<ResultView *>(widget(index)))
        ++index;
    m_plan = new PlanView(this);
    insertTab(index, m_plan, tr("Plan"));
    m_pinPlanButton = m_plan->addHeaderAction(m_pinPlan);
    connect(m_plan, &PlanView::changed, this, [this] { setPinEnabled(m_pinEnabled); });
    removeCloseButton(tabBar(), index);
}

bool ResultPanel::isFixed(QWidget *tab) const
{
    return tab == m_results || tab == m_plan || tab == m_messages;
}

QList<ResultView *> ResultPanel::pinnedResults() const
{
    QList<ResultView *> views;
    for (int i = 0; i < count(); ++i) {
        if (auto *view = qobject_cast<ResultView *>(widget(i)); view && view != m_results)
            views << view;
    }
    return views;
}

QList<PlanView *> ResultPanel::pinnedPlans() const
{
    QList<PlanView *> views;
    for (int i = 0; i < count(); ++i) {
        if (auto *view = qobject_cast<PlanView *>(widget(i)); view && view != m_plan)
            views << view;
    }
    return views;
}

void ResultPanel::addPinned(QWidget *view, const QString &title, QWidget *before)
{
    m_names.insert(view, {title.simplified(), {}});
    const int index = insertTab(indexOf(before), view, m_pin->icon(), QString());
    setPinnedName(view, {});
    setCurrentIndex(index);
}

QString ResultPanel::pinnedName(QWidget *view) const
{
    const PinnedName names = m_names.value(view);
    return names.name.isEmpty() ? names.title : names.name;
}

void ResultPanel::setPinnedName(QWidget *view, const QString &name)
{
    const auto it = m_names.find(view);
    if (it == m_names.end())
        return;
    it->name = name.simplified();
    const int index = indexOf(view);
    setTabText(index, fontMetrics().elidedText(pinnedName(view), Qt::ElideRight, 200));
    setTabToolTip(index, it->title);
}

QLineEdit *ResultPanel::renamePinned(QWidget *view)
{
    if (!m_names.contains(view))
        return nullptr;
    setCurrentWidget(view);
    auto *edit = new TabNameEdit(pinnedName(view), tabBar());
    const QRect tab = tabBar()->tabRect(indexOf(view));
    edit->setGeometry(tab.x(), tab.y(), std::max(tab.width(), 200), tab.height());
    edit->selectAll();
    edit->show();
    edit->setFocus();

    const QPointer<QWidget> target = view;
    auto done = [this, edit, target](bool keep) {
        edit->disconnect(this);
        if (keep && target && m_names.contains(target))
            setPinnedName(target, edit->text());
        edit->deleteLater();
        if (target)
            target->setFocus();
    };
    connect(edit, &QLineEdit::editingFinished, this, [done] { done(true); });
    connect(edit, &TabNameEdit::cancelled, this, [done] { done(false); });
    // A tab closed or moved while its name is edited takes the editor along.
    connect(this, &QTabWidget::currentChanged, edit, [done] { done(false); });
    return edit;
}

ResultView *ResultPanel::pin()
{
    if (!m_pinEnabled || !m_results->showsRows())
        return nullptr;
    ResultView *pinned = m_results;
    delete m_pinButton;
    m_pinButton = nullptr;
    removeTab(indexOf(pinned));
    newResults();
    addPinned(pinned, pinned->title(), m_plan);
    setPinEnabled(m_pinEnabled);
    Q_EMIT resultsReplaced(m_results);
    return pinned;
}

PlanView *ResultPanel::pinPlan()
{
    if (!m_pinEnabled || !m_plan->hasPlan())
        return nullptr;
    PlanView *pinned = m_plan;
    delete m_pinPlanButton;
    m_pinPlanButton = nullptr;
    removeTab(indexOf(pinned));
    newPlan();
    // Named apart from pinned rows of the same statement.
    addPinned(pinned, tr("Plan: %1").arg(pinned->title()), m_messages);
    setPinEnabled(m_pinEnabled);
    return pinned;
}

void ResultPanel::setPinEnabled(bool enabled)
{
    m_pinEnabled = enabled;
    m_pin->setEnabled(enabled && m_results->showsRows());
    m_pinPlan->setEnabled(enabled && m_plan->hasPlan());
}

void ResultPanel::closePinned(QWidget *view)
{
    if (!view || isFixed(view) || indexOf(view) < 0)
        return;
    if (auto *results = qobject_cast<ResultView *>(view))
        Q_EMIT pinnedClosing(results);
    m_names.remove(view);
    removeTab(indexOf(view));
    view->deleteLater();
}

void ResultPanel::showTabMenu(const QPoint &pos)
{
    QWidget *tab = widget(tabBar()->tabAt(pos));
    if (!tab || tab == m_messages)
        return;
    QMenu menu(this);
    if (tab == m_results) {
        menu.addAction(m_pin);
    } else if (tab == m_plan) {
        menu.addAction(m_pinPlan);
    } else {
        menu.addAction(tr("&Rename…"), this, [this, tab] { renamePinned(tab); });
        menu.addAction(tr("&Close"), this, [this, tab] { closePinned(tab); });
    }
    QList<QWidget *> pinned;
    for (int i = 0; i < count(); ++i) {
        if (!isFixed(widget(i)))
            pinned << widget(i);
    }
    menu.addAction(tr("Close &All Pinned"), this,
                   [this, pinned] {
                       for (QWidget *w : pinned)
                           closePinned(w);
                   })
        ->setEnabled(!pinned.isEmpty());
    menu.exec(tabBar()->mapToGlobal(pos));
}

void ResultPanel::showResults()
{
    setCurrentWidget(m_results);
}

void ResultPanel::showPlan()
{
    setCurrentWidget(m_plan);
}

void ResultPanel::showMessages()
{
    setCurrentWidget(m_messages);
}

void ResultPanel::log(const QString &text, bool error)
{
    const QString line
        = QTime::currentTime().toString(QStringLiteral("HH:mm:ss")) + QStringLiteral("  ") + text;
    if (error)
        m_messages->appendHtml(
            QStringLiteral("<span style=\"color:#c62828\">%1</span>")
                .arg(line.toHtmlEscaped().replace(QLatin1Char('\n'), QStringLiteral("<br>"))));
    else
        m_messages->appendPlainText(line);
}

} // namespace sqliteman

#include "ResultPanel.moc"
