// SPDX-FileCopyrightText: 2026 Petr Vanek
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "sql/TextSearch.h"

#include <QTimer>
#include <QWidget>

#include <memory>

class QAction;
class QComboBox;

namespace Ui {
class SearchBar;
}

namespace sqliteman {

class SqlEditor;

// Find and replace for an editor, in a bar below it as in Kate. Ctrl+F
// opens it for incremental search, which finds as you type; Ctrl+R opens
// it with replace and the search options, and searches on Enter. F3 and
// Shift+F3 find again, bar open or not.
class SearchBar : public QWidget
{
    Q_OBJECT

public:
    enum class Mode { Incremental, Power };
    // What the find field shows by its color.
    enum class State { None, Found, NotFound, Invalid };

    explicit SearchBar(SqlEditor *editor, QWidget *parent = nullptr);
    ~SearchBar() override;

    // Find, Replace, Find Next and Previous, Find Selected and backwards:
    // for the widget holding the editor and this bar, so that they work
    // while the bar is hidden too.
    QList<QAction *> editorActions() const { return m_actions; }
    QAction *findAction() const { return m_actions[0]; }
    QAction *replaceAction() const { return m_actions[1]; }

    Mode mode() const { return m_mode; }
    State state() const { return m_state; }
    QString statusText() const;
    QComboBox *findField() const;
    QComboBox *replaceField() const;
    // What the bar searches for as it is set now.
    sql::TextSearch search() const;

    // Opens the bar, with the selection, if on one line, as the text to find.
    void openIncremental();
    // Opens it with replace; a selection over several lines becomes the
    // only part searched.
    void openPower();
    void closeBar();
    // The next or previous match from the selection, starting over at the
    // other end if there is none. False if none at all.
    bool findNext();
    bool findPrevious();
    // Searches for the selected text, or the word at the cursor.
    bool findSelected(bool backward);
    // Replaces the selected text if it is a match, then finds the next one.
    void replace();
    int replaceAll();
    int findAll();

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;

private:
    // Opens the bar in the mode, or closes it if it is in that mode already
    // and has the focus, for Ctrl+F and Ctrl+R to close what they opened.
    void toggle(Mode mode);
    void setMode(Mode mode);
    // The selected text as the text to find, if any; false if none was taken.
    bool takeSelection(bool forPower);
    // Searches from where the bar opened as the text to find changes.
    void incrementalSearch();
    bool find(bool backward, const sql::TextSearch &search);
    // Where to search: the scope, or the whole text.
    std::pair<qsizetype, qsizetype> range() const;
    void setScopeFromSelection();
    void highlight(const sql::TextSearch &search);
    void clearHighlight();
    void setState(State state, const QString &message = {});
    void remember();
    QString newline() const;

    std::unique_ptr<Ui::SearchBar> m_ui; // Layout in SearchBar.ui.
    SqlEditor *m_editor = nullptr;
    QList<QAction *> m_actions;
    Mode m_mode = Mode::Incremental;
    State m_state = State::None;
    qsizetype m_start = 0; // Where incremental search goes from.
    bool m_highlighting = false; // Matches are on show, to keep up to date.
    sql::TextSearch m_highlighted; // What they are matches of.
    QTimer m_highlightTimer;
};

} // namespace sqliteman
