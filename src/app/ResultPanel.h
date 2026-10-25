// SPDX-FileCopyrightText: 2026 Petr Vanek
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QHash>
#include <QTabWidget>

class QAction;
class QLineEdit;
class QPlainTextEdit;
class QToolButton;

namespace sqliteman {

class PlanView;
class ResultView;

// The bottom pane for one editor: result rows, EXPLAIN plan and messages.
// Each statement's rows replace the last ones under Results, and each plan
// the last one under Plan, unless that was pinned: then it keeps a tab of
// its own until it is closed.
class ResultPanel : public QTabWidget
{
    Q_OBJECT

public:
    explicit ResultPanel(QWidget *parent = nullptr);

    // Where the next statement's rows go.
    ResultView *results() const { return m_results; }
    QList<ResultView *> pinnedResults() const;
    // Where the next plan goes.
    PlanView *plan() const { return m_plan; }
    QList<PlanView *> pinnedPlans() const;
    QPlainTextEdit *messages() const { return m_messages; }

    // Moves the rows under Results to a tab of their own, and puts an empty
    // view under Results. Returns the pinned view; null if there was
    // nothing to pin.
    ResultView *pin();
    // The same for the plan under Plan.
    PlanView *pinPlan();
    QAction *pinAction() const { return m_pin; }
    QAction *pinPlanAction() const { return m_pinPlan; }
    // Whoever runs the statements says when the rows are done arriving.
    void setPinEnabled(bool enabled);
    // Closes a pinned tab; Results, Plan and Messages stay.
    void closePinned(QWidget *view);
    // A pinned tab's name, as the user gave it or else from its statement.
    QString pinnedName(QWidget *view) const;
    // An empty name goes back to the one from the statement.
    void setPinnedName(QWidget *view, const QString &name);
    // Edits the name in place, over the tab: Enter keeps it, Esc does not.
    // Returns the editor; null for a tab that is not pinned.
    QLineEdit *renamePinned(QWidget *view);

    void showResults();
    void showPlan();
    void showMessages();
    // Adds a line to Messages, with the time.
    void log(const QString &text, bool error = false);

Q_SIGNALS:
    // A new view under Results, after pin(), to connect like the first.
    void resultsReplaced(sqliteman::ResultView *view);
    // A pinned view is about to go: whatever it waits for is not wanted.
    void pinnedClosing(sqliteman::ResultView *view);

private:
    void newResults();
    void newPlan();
    // Puts a view in a tab of its own, after the others pinned before it.
    void addPinned(QWidget *view, const QString &title, QWidget *before);
    bool isFixed(QWidget *tab) const;
    void showTabMenu(const QPoint &pos);

    ResultView *m_results = nullptr;
    QToolButton *m_pinButton = nullptr; // In the header of the view under Results.
    QAction *m_pin = nullptr;
    bool m_pinEnabled = false;
    PlanView *m_plan = nullptr;
    QToolButton *m_pinPlanButton = nullptr;
    QAction *m_pinPlan = nullptr;
    struct PinnedName
    {
        QString title; // From the statement; the tooltip, whatever the name.
        QString name; // Given by the user; empty for none.
    };
    QHash<const QWidget *, PinnedName> m_names;
    QPlainTextEdit *m_messages = nullptr;
};

} // namespace sqliteman
