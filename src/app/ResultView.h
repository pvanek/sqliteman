// SPDX-FileCopyrightText: 2026 Petr Vanek
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "catalog/Export.h"
#include "sqlite/QueryRunner.h"

#include <QPointer>
#include <QSet>
#include <QWidget>

#include <functional>

class QAction;
class QHBoxLayout;
class QLabel;
class QStackedWidget;
class QTabBar;
class QTableView;
class QToolButton;

namespace sqliteman {

class RecordModel;
class ResultTextView;
class ResultModel;
class ValueViewer;

// Shows the rows of a query, or its error. Either runs the query itself
// (run()), or is fed by whoever runs it (begin(), append(), finish()).
class ResultView : public QWidget
{
    Q_OBJECT

public:
    explicit ResultView(QWidget *parent = nullptr);

    // Runs sql and shows its last result. Refresh runs it again.
    void run(sqlite::QueryRunner *runner, const QString &title, const QByteArray &sql);
    QByteArray sql() const { return m_sql; }

    void begin(const QString &title);
    // What begin() or run() named the rows, the query as a rule.
    QString title() const;
    void append(const sqlite::Result &result);
    void finish(const QString &status);
    void showError(const QString &message);
    void showMessage(const QString &text);
    // What the Run Again button does; none disables it.
    void setRerun(std::function<void()> rerun);
    // Clicking a column's header sorts by it, again the other way round, a
    // third time back to the query's own order. For rows nobody edits.
    void setSortable(bool sortable);

    // How the rows are shown.
    enum class ViewMode {
        Grid, // The editable table.
        Text, // An ASCII table, to copy into a file or a message.
        Record // One row at a time, its columns under each other.
    };
    void setViewMode(ViewMode mode);
    ViewMode viewMode() const { return m_mode; }

    // Writes the rows the options choose. Returns false and sets error when
    // the file cannot be written.
    bool exportTo(const catalog::ExportOptions &options, const QString &path,
                  QString *error = nullptr) const;
    catalog::ExportOptions exportOptions(catalog::ExportFormat format) const;
    // The rows selected in the grid, in order.
    // The selected rows, as rows of model()->rows(), in the order shown.
    std::vector<int> selectedRows() const;
    // False when the query has more rows than were fetched, which makes
    // "all rows" an export that runs the query again.
    void setAllRowsFetched(bool all);
    bool allRowsFetched() const { return m_allFetched; }
    // Opens the export dialog, as the Export button does.
    void exportWithDialog();
    // Puts the rows on the clipboard: the selected ones, or all of them.
    void copyAs(catalog::ExportFormat format);

    ResultModel *model() const { return m_model; }
    QTableView *table() const { return m_table; }
    ResultTextView *textView() const { return m_text; }
    // The editing palette and the button that shows it.
    QToolButton *editToggle() const { return m_editToggle; }
    QWidget *editBar() const { return m_editBar; }
    QTabBar *modeTabs() const { return m_modeTabs; }
    QTableView *recordView() const { return m_record; }
    bool isRunning() const { return m_running; }
    // A button in the header, before Run Again, for what whoever holds the
    // view adds to it.
    QToolButton *addHeaderAction(QAction *action);
    void addHeaderWidget(QWidget *widget);
    // An entry in the menu of the grid and the record view, after the view's
    // own; hidden while the action is.
    void addContextAction(QAction *action);
    // Opens a cell's whole value in a window of its own, to edit it when
    // the cell is editable. The index is the grid's, of model(); null for a
    // cell left to its default.
    ValueViewer *viewValue(const QModelIndex &index);
    // View Value, for the cell under the cursor in the grid or record view.
    QAction *viewValueAction() const { return m_viewValue; }
    // When run() runs the query again, the rows selected stay selected by
    // the value in this column; column widths and scrolling stay anyway.
    void setKeyColumn(const QString &name) { m_keyColumn = name; }
    // The column's index in the rows on show, or -1.
    int columnIndex(const QString &name) const;
    // A line beside the row count, until the next one.
    void setNote(const QString &text, bool error = false);
    QString note() const;
    // Rows are on show, rather than a message or nothing yet.
    bool showsRows() const;
    // Cancels the running query, if it is one this view runs itself.
    void stop();
    // Columns as wide as their contents, within limits, and never narrower
    // than their header's full text.
    void sizeColumns();

Q_SIGNALS:
    void finished();
    // run() has put a query's rows on show, after it ran again too.
    void rowsShown();
    // Every row is wanted but not all of them were fetched: whoever ran the
    // query runs it again and writes the rows as they arrive.
    void exportAllRequested(const sqliteman::catalog::ExportOptions &options, const QString &path);
    // Save was asked for; whoever ran the query saves the model's changes.
    void saveRequested();

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    // The selected cells' whole values on the clipboard.
    void copyCells(QTableView *view);
    void rerunQuery();
    void showOutcome(const sqlite::QueryOutcome &outcome, qint64 elapsedMs);
    // What run() keeps of the view when it shows the rows again.
    struct Layout
    {
        QStringList columns;
        QList<int> widths;
        int horizontal = 0;
        int vertical = 0;
        QSet<QByteArray> selectedKeys;
    };
    Layout layout() const;
    void restore(const Layout &layout);
    void setMessage(const QString &text, bool error);
    void updateEditing();
    // What the header section needs for its own text, ignoring the data.
    int headerWidth(int column) const;
    // Fills the view the current mode shows; force leaves a message behind
    // even before rows have arrived.
    void updateView(bool force = false);
    void updateRecord(); // The row the record view shows.
    void stepRecord(int by); // Moves to another row in the record view.
    void refresh();
    // The grid's cell for an index of the grid or of the record view.
    QModelIndex gridIndex(const QModelIndex &index) const;
    void showContextMenu(QTableView *view, const QPoint &pos);

    QHBoxLayout *m_header = nullptr;
    QLabel *m_title = nullptr;
    QLabel *m_status = nullptr;
    QLabel *m_note = nullptr;
    QString m_keyColumn;
    QToolButton *m_refresh = nullptr;
    QToolButton *m_stop = nullptr;
    QStackedWidget *m_stack = nullptr;
    QTableView *m_table = nullptr;
    ResultTextView *m_text = nullptr;
    QTableView *m_record = nullptr;
    RecordModel *m_recordModel = nullptr;
    QTabBar *m_modeTabs = nullptr;
    QLabel *m_recordLabel = nullptr; // "Row 3 of 42" under the record form.
    QToolButton *m_editToggle = nullptr;
    QToolButton *m_export = nullptr;
    QAction *m_previousRow = nullptr;
    QAction *m_nextRow = nullptr;
    QLabel *m_message = nullptr;
    ResultModel *m_model = nullptr;
    QLabel *m_editLabel = nullptr;
    QWidget *m_editBar = nullptr;
    QAction *m_addRow = nullptr;
    QAction *m_deleteRows = nullptr;
    QAction *m_setNull = nullptr;
    QAction *m_save = nullptr;
    QAction *m_discard = nullptr;
    QAction *m_rerunAction = nullptr;
    QAction *m_viewValue = nullptr;
    QList<QAction *> m_contextActions;

    std::function<void()> m_rerun;
    QPointer<sqlite::QueryRunner> m_runner;
    QByteArray m_sql;
    quint64 m_job = 0; // What run() has running, to stop it.
    quint64 m_generation = 0; // Ignores results of queries run before the latest one.
    bool m_running = false;
    bool m_sized = false; // Columns sized to their contents once rows arrived.
    ViewMode m_mode = ViewMode::Grid;
    bool m_textStale = true; // The text view is rendered when it is shown.
    bool m_allFetched = true;
    // The text view renders this many rows at most; an export writes them all.
    static constexpr int MaxTextRows = 10000;
    // Long texts and JSON would take the whole view; headers get more room
    // because a column nobody can name is of no use.
    static constexpr int MaxContentWidth = 400;
    static constexpr int MaxHeaderWidth = 600;
};

} // namespace sqliteman
