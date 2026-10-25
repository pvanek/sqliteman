// SPDX-FileCopyrightText: 2026 Petr Vanek
// SPDX-License-Identifier: GPL-3.0-or-later

#include "ResultView.h"

#include "ExportDialog.h"
#include "ResultModel.h"
#include "ResultTextView.h"
#include "Settings.h"
#include "Shortcuts.h"
#include "ValueViewer.h"

#include <QAction>
#include <QApplication>
#include <QClipboard>
#include <QFileDialog>
#include <QGuiApplication>
#include <QMenu>
#include <QSaveFile>
#include <QTextStream>
#include <QElapsedTimer>
#include <QItemSelectionModel>
#include <QKeyEvent>
#include <QToolBar>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QMessageBox>
#include <QPainter>
#include <QScrollBar>
#include <QStackedWidget>
#include <QStyle>
#include <QStyleOptionHeader>
#include <QStyledItemDelegate>
#include <QTabBar>
#include <QTextLayout>
#include <QTableView>
#include <QToolButton>
#include <QVBoxLayout>

#include <algorithm>
#include <cmath>

namespace sqliteman {

namespace {

// Wraps at word boundaries where it can and anywhere where it cannot: the
// style's own wrapping only breaks between words and elides the rest, and
// a JSON string or a hash can run for thousands of characters without one.
class WrappingDelegate : public QStyledItemDelegate
{
public:
    using QStyledItemDelegate::QStyledItemDelegate;

    void paint(QPainter *painter, const QStyleOptionViewItem &option,
               const QModelIndex &index) const override
    {
        QStyleOptionViewItem opt(option);
        initStyleOption(&opt, index);
        QTextLayout layout;
        const qreal height = lay(opt, layout);
        opt.text.clear(); // The background, the selection and the focus only.
        style(opt)->drawControl(QStyle::CE_ItemViewItem, &opt, painter, opt.widget);

        const QRect text = textRect(opt);
        const bool selected = opt.state & QStyle::State_Selected;
        painter->save();
        painter->setPen(opt.palette.color(opt.state & QStyle::State_Enabled ? QPalette::Normal
                                                                            : QPalette::Disabled,
                                          selected ? QPalette::HighlightedText : QPalette::Text));
        painter->setClipRect(text);
        // In the middle, as the name beside it.
        const qreal top = text.top() + std::max(0.0, (text.height() - height) / 2);
        layout.draw(painter, QPointF(text.left(), top));
        painter->restore();
    }

    QSize sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const override
    {
        // The table sets the column's width into the option when it wraps.
        QStyleOptionViewItem opt(option);
        initStyleOption(&opt, index);
        QTextLayout layout;
        const int height = int(std::ceil(lay(opt, layout)));
        return {opt.rect.width(),
                std::max(height + 2 * margin(opt, QStyle::PM_FocusFrameVMargin),
                         opt.fontMetrics.height() + 6)};
    }

private:
    static QStyle *style(const QStyleOptionViewItem &opt)
    {
        return opt.widget ? opt.widget->style() : QApplication::style();
    }
    // The space the style leaves around an item's text, here below it.
    static int margin(const QStyleOptionViewItem &opt, QStyle::PixelMetric metric)
    {
        return style(opt)->pixelMetric(metric, nullptr, opt.widget) + 1;
    }
    // Inset at the sides only, as the style does it.
    static QRect textRect(const QStyleOptionViewItem &opt)
    {
        const int h = margin(opt, QStyle::PM_FocusFrameHMargin);
        return opt.rect.adjusted(h, 0, -h, 0);
    }
    // Lays the text out in the item's width; returns its height.
    static qreal lay(const QStyleOptionViewItem &opt, QTextLayout &layout)
    {
        QString text = opt.text;
        layout.setText(text.replace(QLatin1Char('\n'), QChar::LineSeparator));
        layout.setFont(opt.font);
        QTextOption wrap;
        wrap.setWrapMode(QTextOption::WrapAtWordBoundaryOrAnywhere);
        layout.setTextOption(wrap);
        const qreal width = std::max(1, textRect(opt).width());
        qreal height = 0;
        layout.beginLayout();
        for (QTextLine line = layout.createLine(); line.isValid(); line = layout.createLine()) {
            line.setLineWidth(width);
            line.setPosition(QPointF(0, height));
            height += line.height();
        }
        layout.endLayout();
        return height;
    }
};

} // namespace

// One row of a result, its columns under each other: name, then value.
class RecordModel : public QAbstractTableModel
{
public:
    explicit RecordModel(ResultModel *rows, QObject *parent = nullptr)
        : QAbstractTableModel(parent), m_rows(rows)
    { }

    void setRow(int row)
    {
        beginResetModel();
        m_row = row;
        endResetModel();
    }
    int row() const { return m_row; }

    int rowCount(const QModelIndex &parent = {}) const override
    {
        return parent.isValid() || m_row < 0 ? 0 : m_rows->columnCount();
    }
    int columnCount(const QModelIndex &parent = {}) const override
    {
        return parent.isValid() ? 0 : 2;
    }

    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override
    {
        if (!index.isValid() || m_row < 0 || m_row >= m_rows->rowCount())
            return {};
        if (index.column() == 0) {
            if (role == Qt::DisplayRole || role == ResultModel::ValueRole)
                return m_rows->headerData(index.row(), Qt::Horizontal);
            if (role == Qt::ForegroundRole)
                return QVariant(); // The style's own color.
            return {};
        }
        // The value as the grid shows it, NULLs and all, except that here
        // there is room for all of it, line breaks too.
        const QModelIndex cell = m_rows->index(m_row, index.row());
        if (role == Qt::DisplayRole) {
            const QVariant whole = m_rows->data(cell, ResultModel::ValueRole);
            if (whole.isValid())
                return whole;
        }
        if (role == Qt::ToolTipRole)
            return {}; // Nothing the cell does not show already.
        return m_rows->data(cell, role);
    }

    QVariant headerData(int section, Qt::Orientation orientation, int role) const override
    {
        if (orientation != Qt::Horizontal || role != Qt::DisplayRole)
            return {};
        return section == 0 ? tr("Column") : tr("Value");
    }

private:
    ResultModel *m_rows = nullptr;
    int m_row = -1;
};

ResultView::ResultView(QWidget *parent)
    : QWidget(parent), m_title(new QLabel(this)), m_status(new QLabel(this)),
      m_refresh(new QToolButton(this)), m_stop(new QToolButton(this)),
      m_stack(new QStackedWidget(this)), m_table(new QTableView(this)), m_message(new QLabel(this)),
      m_model(new ResultModel(this))
{
    QFont bold = m_title->font();
    bold.setBold(true);
    m_title->setFont(bold);
    m_refresh->setIcon(QIcon::fromTheme(QStringLiteral("view-refresh")));
    m_refresh->setToolTip(tr("Run again"));
    m_refresh->setAutoRaise(true);
    // Run again: the button, F5 and Ctrl+R (Cmd+R on macOS) while in here.
    m_rerunAction
        = new QAction(QIcon::fromTheme(QStringLiteral("view-refresh")), tr("Run Again"), this);
    m_rerunAction->setShortcuts(Shortcuts::refresh());
    m_rerunAction->setShortcutContext(Qt::WidgetWithChildrenShortcut);
    m_rerunAction->setEnabled(false);
    addAction(m_rerunAction);
    connect(m_rerunAction, &QAction::triggered, this, &ResultView::refresh);
    m_refresh->setDefaultAction(m_rerunAction);
    // Stop: while a query this view runs itself is running.
    m_stop->setIcon(QIcon::fromTheme(QStringLiteral("process-stop")));
    m_stop->setText(tr("Stop"));
    m_stop->setToolTip(tr("Stop the query"));
    m_stop->setAutoRaise(true);
    m_stop->hide();
    connect(m_stop, &QToolButton::clicked, this, &ResultView::stop);

    // Editing: only shown when the result can be written back.
    m_editLabel = new QLabel(this);
    auto *editBar = new QToolBar(this);
    editBar->setIconSize(QSize(16, 16));
    editBar->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    auto action
        = [&](const QString &icon, const QString &text, const QKeySequence &key, auto slot) {
              auto *a = editBar->addAction(QIcon::fromTheme(icon), text);
              a->setShortcut(key);
              a->setShortcutContext(Qt::WidgetWithChildrenShortcut);
              m_table->addAction(a);
              connect(a, &QAction::triggered, this, slot);
              return a;
          };
    m_addRow = action(QStringLiteral("list-add"), tr("Add Row"),
                      QKeySequence(Qt::CTRL | Qt::Key_Insert), [this] {
                          const int row = m_model->addRow();
                          m_table->setCurrentIndex(m_model->index(row, 0));
                          m_table->scrollToBottom();
                      });
    m_deleteRows
        = action(QStringLiteral("list-remove"), tr("Delete Rows"), QKeySequence::Delete, [this] {
              QList<int> rows;
              for (const QModelIndex &i : m_table->selectionModel()->selectedIndexes()) {
                  if (!rows.contains(i.row()))
                      rows << i.row();
              }
              m_model->toggleDeleted(rows);
          });
    m_setNull = action(QStringLiteral("edit-clear"), tr("Set NULL"),
                       QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_N),
                       [this] { m_model->setNull(m_table->selectionModel()->selectedIndexes()); });
    // No shortcut of its own: the main window's Ctrl+S saves these changes
    // when the grid has focus, and the script otherwise.
    m_save = action(QStringLiteral("document-save"), tr("Save"), QKeySequence(),
                    [this] { Q_EMIT saveRequested(); });
    m_discard = action(QStringLiteral("edit-undo"), tr("Discard"), QKeySequence(),
                       [this] { m_model->discardChanges(); });
    m_editBar = editBar;
    connect(m_model, &ResultModel::changesChanged, this, &ResultView::updateEditing);
    connect(m_model, &ResultModel::editTargetChanged, this, &ResultView::updateEditing);

    // The palette of editing tools: out of the way until it is wanted.
    m_editToggle = new QToolButton(this);
    m_editToggle->setIcon(QIcon::fromTheme(QStringLiteral("document-edit")));
    m_editToggle->setText(tr("Edit"));
    m_editToggle->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    m_editToggle->setToolTip(tr("Show the editing tools"));
    m_editToggle->setCheckable(true);
    m_editToggle->setAutoRaise(true);
    connect(m_editToggle, &QToolButton::toggled, this, [this] { updateEditing(); });

    // Grid, text or one row at a time: tabs down the left side.
    m_modeTabs = new QTabBar(this);
    m_modeTabs->setShape(QTabBar::RoundedWest);
    m_modeTabs->setExpanding(false);
    m_modeTabs->setDrawBase(false);
    m_modeTabs->addTab(tr("Grid"));
    m_modeTabs->addTab(tr("Text"));
    m_modeTabs->addTab(tr("Record"));
    m_modeTabs->setTabToolTip(0, tr("The rows in a table, editable"));
    m_modeTabs->setTabToolTip(1, tr("The rows as text, to copy out; Alt+drag selects a block"));
    m_modeTabs->setTabToolTip(2, tr("One row at a time"));
    connect(m_modeTabs, &QTabBar::currentChanged, this,
            [this](int index) { setViewMode(ViewMode(index)); });

    // Arrows from the style when the icon theme has none, so the buttons
    // stay small either way.
    m_previousRow = new QAction(
        QIcon::fromTheme(QStringLiteral("go-up"), style()->standardIcon(QStyle::SP_ArrowUp)),
        tr("Previous Row"), this);
    m_nextRow = new QAction(
        QIcon::fromTheme(QStringLiteral("go-down"), style()->standardIcon(QStyle::SP_ArrowDown)),
        tr("Next Row"), this);
    connect(m_previousRow, &QAction::triggered, this, [this] { stepRecord(-1); });
    connect(m_nextRow, &QAction::triggered, this, [this] { stepRecord(1); });

    m_export = new QToolButton(this);
    m_export->setIcon(QIcon::fromTheme(QStringLiteral("document-save-as")));
    m_export->setText(tr("Export"));
    m_export->setToolTip(tr("Save or copy the rows"));
    m_export->setAutoRaise(true);
    m_export->setPopupMode(QToolButton::InstantPopup);
    auto *menu = new QMenu(m_export);
    // The dialog covers every combination; these are the everyday ones,
    // which take the selected rows when there are any.
    menu->addAction(tr("&Export…"), this, [this] { exportWithDialog(); });
    menu->addSeparator();
    using catalog::ExportFormat;
    for (const ExportFormat format : {ExportFormat::Csv, ExportFormat::Json, ExportFormat::Markdown,
                                      ExportFormat::Text, ExportFormat::SqlInsert}) {
        menu->addAction(tr("Copy as %1").arg(catalog::formatName(format)), this,
                        [this, format] { copyAs(format); });
    }
    m_export->setMenu(menu);

    auto *header = new QHBoxLayout;
    m_header = header;
    header->setContentsMargins(4, 2, 4, 2);
    header->addWidget(m_title);
    header->addStretch();
    header->addWidget(m_editToggle);
    header->addWidget(m_export);
    header->addWidget(m_stop);
    header->addWidget(m_refresh);

    m_table->setModel(m_model);
    m_table->setAlternatingRowColors(true);
    m_table->setSelectionBehavior(QAbstractItemView::SelectItems);
    m_table->horizontalHeader()->setSectionResizeMode(QHeaderView::Interactive);
    m_table->verticalHeader()->setDefaultSectionSize(m_table->fontMetrics().height() + 6);
    // The first chunk brings the columns and resets the model; later ones
    // insert rows. Either way the columns are sized once, when rows are there.
    auto sizeOnce = [this] {
        if (m_sized || !m_model->hasColumns())
            return;
        m_sized = m_model->rowCount() > 0; // Headers alone: size again with the rows.
        sizeColumns();
    };
    connect(m_model, &ResultModel::modelReset, this, sizeOnce);
    connect(m_model, &ResultModel::rowsInserted, this, sizeOnce);

    m_message->setWordWrap(true);
    m_message->setAlignment(Qt::AlignTop | Qt::AlignLeft);
    m_message->setTextInteractionFlags(Qt::TextSelectableByMouse);
    m_message->setMargin(8);

    // The text view is a Scintilla: it can select a block of columns.
    m_text = new ResultTextView(this);

    m_recordModel = new RecordModel(m_model, this);
    m_record = new QTableView(this);
    m_record->setModel(m_recordModel);
    m_record->setAlternatingRowColors(true);
    m_record->verticalHeader()->hide();
    m_record->horizontalHeader()->setStretchLastSection(true);
    m_record->setEditTriggers(QAbstractItemView::NoEditTriggers);
    // Each row as tall as its whole value, wrapped, and scrolled through
    // smoothly when that is taller than the view.
    m_record->setWordWrap(true);
    m_record->setItemDelegateForColumn(1, new WrappingDelegate(m_record));
    m_record->verticalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    m_record->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);

    // The views' own copy would take only the current cell, and its
    // shortened text at that. The key is caught in the view itself rather
    // than left to the action's shortcut: on Wayland a shortcut of a widget
    // can miss the key, which then reaches the view's own copy. Only while
    // the view itself has focus, so a cell editor keeps its own.
    for (QTableView *view : {m_table, m_record}) {
        auto *copy = new QAction(QIcon::fromTheme(QStringLiteral("edit-copy")), tr("Copy"), view);
        copy->setObjectName(QStringLiteral("copy"));
        copy->setShortcuts(QKeySequence::Copy); // Shown in the menu; eventFilter() takes the key.
        copy->setShortcutContext(Qt::WidgetShortcut);
        view->addAction(copy);
        connect(copy, &QAction::triggered, this, [this, view] { copyCells(view); });
        view->installEventFilter(this);
        view->setContextMenuPolicy(Qt::CustomContextMenu);
        connect(view, &QWidget::customContextMenuRequested, this,
                [this, view](const QPoint &pos) { showContextMenu(view, pos); });
    }

    // A value too long, or too nested, for its cell. Shift+Enter as in
    // other database tools; double-click too, where it does not edit.
    m_viewValue = new QAction(QIcon::fromTheme(QStringLiteral("document-preview")),
                              tr("View Value…"), this);
    // Shown in the menus; eventFilter() takes the key, as it does Copy.
    m_viewValue->setShortcut(QKeySequence(Qt::SHIFT | Qt::Key_Return));
    m_viewValue->setShortcutContext(Qt::WidgetShortcut);
    m_table->addAction(m_viewValue);
    m_record->addAction(m_viewValue);
    connect(m_viewValue, &QAction::triggered, this, [this] {
        QTableView *shown = m_mode == ViewMode::Record ? m_record : m_table;
        viewValue(gridIndex(shown->currentIndex()));
    });
    connect(m_table, &QTableView::doubleClicked, this, [this](const QModelIndex &index) {
        if (m_table->editTriggers() == QAbstractItemView::NoEditTriggers)
            viewValue(index);
    });
    connect(m_record, &QTableView::doubleClicked, this,
            [this](const QModelIndex &index) { viewValue(gridIndex(index)); });

    // The record view follows the grid, and the grid the record view.
    connect(m_table->selectionModel(), &QItemSelectionModel::currentChanged, this,
            [this](const QModelIndex &current) {
                if (m_mode == ViewMode::Record || current.isValid())
                    updateRecord();
            });
    connect(m_model, &ResultModel::modelReset, this, [this] {
        m_textStale = true;
        updateView();
    });
    connect(m_model, &ResultModel::rowsInserted, this, [this] { m_textStale = true; });
    // Sorted: the text shows the new order, the record view its row's place.
    connect(m_model, &ResultModel::layoutChanged, this, [this] {
        m_textStale = true;
        if (m_mode != ViewMode::Grid)
            updateView();
    });
    connect(m_model, &ResultModel::dataChanged, this, [this] {
        m_textStale = true;
        if (m_mode == ViewMode::Record)
            updateRecord();
    });

    // The record form and its own row navigation, under it.
    auto *recordPage = new QWidget(this);
    auto *recordLayout = new QVBoxLayout(recordPage);
    recordLayout->setContentsMargins(0, 0, 0, 0);
    recordLayout->setSpacing(0);
    recordLayout->addWidget(m_record);
    auto *recordNav = new QToolBar(recordPage);
    recordNav->setIconSize(QSize(16, 16));
    recordNav->addAction(m_previousRow);
    recordNav->addAction(m_nextRow);
    m_recordLabel = new QLabel(recordPage);
    auto *navLayout = new QHBoxLayout;
    navLayout->setContentsMargins(4, 0, 4, 2);
    navLayout->addWidget(recordNav);
    navLayout->addWidget(m_recordLabel);
    navLayout->addStretch();
    recordLayout->addLayout(navLayout);

    m_stack->addWidget(m_table);
    m_stack->addWidget(m_text);
    m_stack->addWidget(recordPage);
    m_stack->addWidget(m_message);

    // Row count and how long the query took: out of the way at the bottom.
    m_status->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    auto *footer = new QHBoxLayout;
    footer->setContentsMargins(4, 2, 4, 2);
    footer->addWidget(m_editLabel);
    m_note = new QLabel(this);
    m_note->setTextFormat(Qt::PlainText);
    m_note->hide();
    footer->addWidget(m_note);
    footer->addStretch();
    footer->addWidget(m_status);

    auto *middle = new QHBoxLayout;
    middle->setContentsMargins(0, 0, 0, 0);
    middle->setSpacing(0);
    middle->addWidget(m_modeTabs, 0, Qt::AlignTop);
    middle->addWidget(m_stack, 1);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    layout->addLayout(header);
    layout->addWidget(m_editBar);
    layout->addLayout(middle, 1);
    layout->addLayout(footer);

    showMessage(tr("Run a statement with Ctrl+Enter, or open a table from the database "
                   "browser."));
    updateEditing();
}

void ResultView::updateEditing()
{
    const catalog::EditTarget &target = m_model->editTarget();
    const bool editable = target.editable();
    const bool changes = m_model->hasChanges();
    m_editToggle->setEnabled(editable);
    if (!editable)
        m_editToggle->setChecked(false);
    else if (changes)
        m_editToggle->setChecked(true); // Save and Discard have to be reachable.
    m_editBar->setVisible(editable && m_editToggle->isChecked());
    m_editLabel->setVisible(m_model->hasColumns() && (editable || !target.reason.isEmpty()));
    if (editable) {
        m_editLabel->setText(tr("Editing %1.%2").arg(target.schema, target.table));
        m_editLabel->setToolTip(
            tr("Double-click a cell to change it. Changes are saved with Save."));
    } else {
        m_editLabel->setText(tr("Read-only"));
        m_editLabel->setToolTip(target.reason);
    }
    m_save->setEnabled(changes);
    m_discard->setEnabled(changes);
    m_table->setEditTriggers(editable ? QAbstractItemView::DoubleClicked
                                     | QAbstractItemView::EditKeyPressed
                                     | QAbstractItemView::AnyKeyPressed
                                      : QAbstractItemView::NoEditTriggers);
}

void ResultView::run(sqlite::QueryRunner *runner, const QString &title, const QByteArray &sql)
{
    m_runner = runner;
    m_sql = sql;
    m_title->setText(title);
    setRerun([this] { rerunQuery(); });
    rerunQuery();
}

void ResultView::refresh()
{
    if (!m_rerun || m_running)
        return;
    if (m_model->hasChanges()
        && QMessageBox::question(this, tr("Run Again"),
                                 tr("Running the query again discards the changes not saved yet."),
                                 QMessageBox::Discard | QMessageBox::Cancel)
            != QMessageBox::Discard)
        return;
    m_rerun();
}

void ResultView::rerunQuery()
{
    if (!m_runner) {
        showError(tr("Not connected"));
        return;
    }
    const quint64 generation = ++m_generation;
    m_running = true;
    m_rerunAction->setEnabled(false);
    m_status->setText(tr("Running…"));
    m_stop->show();
    updateView(true);

    QElapsedTimer timer;
    timer.start();
    sqlite::ExecuteOptions options;
    options.maxRows = Settings::instance().preferences().rowLimit;
    m_job = m_runner->run(
        m_sql, this,
        [this, generation, timer](const sqlite::QueryOutcome &outcome) {
            m_job = 0;
            if (generation == m_generation)
                showOutcome(outcome, timer.elapsed());
        },
        options);
}

void ResultView::showOutcome(const sqlite::QueryOutcome &outcome, qint64 elapsedMs)
{
    if (!outcome.ok()) {
        m_model->clear();
        showError(outcome.error);
    } else {
        const Layout before = layout();
        begin(m_title->text());
        const sqlite::Result rows = outcome.rows();
        append(rows.isNull() ? outcome.results.back() : rows);
        restore(before);
        setAllRowsFetched(!rows.isTruncated());
        Q_EMIT rowsShown();
    }
    QString status = outcome.ok()
        ? tr("%n row(s), %1 ms", nullptr, m_model->rowCount()).arg(elapsedMs)
        : tr("%1 ms").arg(elapsedMs);
    if (!m_allFetched)
        status += QLatin1String(" · ") + tr("stopped at the row limit");
    finish(status);
}

void ResultView::begin(const QString &title)
{
    ++m_generation; // Drops the outcome of an earlier run().
    m_title->setText(title);
    m_model->clear();
    m_sized = false;
    m_running = true;
    m_rerunAction->setEnabled(false);
    m_status->setText(tr("Running…"));
    m_textStale = true;
    updateView(true); // Whatever was shown before is gone.
}

QString ResultView::title() const
{
    return m_title->text();
}

QToolButton *ResultView::addHeaderAction(QAction *action)
{
    auto *button = new QToolButton(this);
    button->setDefaultAction(action);
    button->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    button->setAutoRaise(true);
    addHeaderWidget(button);
    return button;
}

void ResultView::addHeaderWidget(QWidget *widget)
{
    widget->setParent(this);
    m_header->insertWidget(m_header->indexOf(m_stop), widget);
}

void ResultView::copyCells(QTableView *view)
{
    const QModelIndexList cells = view->selectionModel()->selectedIndexes();
    if (!cells.isEmpty())
        QGuiApplication::clipboard()->setText(ResultModel::copyText(cells));
}

bool ResultView::eventFilter(QObject *watched, QEvent *event)
{
    auto *view = qobject_cast<QTableView *>(watched);
    if ((view != m_table && view != m_record)
        || (event->type() != QEvent::ShortcutOverride && event->type() != QEvent::KeyPress))
        return QWidget::eventFilter(watched, event);
    const auto *key = static_cast<QKeyEvent *>(event);
    const bool copy = key->matches(QKeySequence::Copy);
    const bool open = key->keyCombination() == QKeyCombination(Qt::SHIFT | Qt::Key_Return)
        || key->keyCombination() == QKeyCombination(Qt::SHIFT | Qt::Key_Enter);
    if (!copy && !open)
        return QWidget::eventFilter(watched, event);
    // Accepting the override keeps any other shortcut in the window from
    // taking the key; it then comes back here as a key press.
    event->accept();
    if (event->type() == QEvent::KeyPress) {
        if (copy)
            copyCells(view);
        else
            viewValue(gridIndex(view->currentIndex()));
    }
    return true;
}

void ResultView::addContextAction(QAction *action)
{
    m_contextActions << action;
}

QModelIndex ResultView::gridIndex(const QModelIndex &index) const
{
    if (index.model() != m_recordModel)
        return index;
    return m_recordModel->row() < 0 ? QModelIndex()
                                    : m_model->index(m_recordModel->row(), index.row());
}

ValueViewer *ResultView::viewValue(const QModelIndex &index)
{
    const QVariant value
        = index.isValid() ? m_model->data(index, ResultModel::RawValueRole) : QVariant();
    if (!value.isValid())
        return nullptr;
    const QString column = m_model->headerData(index.column(), Qt::Horizontal).toString();
    const bool editable = m_model->isWritable(index);
    auto *viewer = new ValueViewer(tr("%1, row %2").arg(column).arg(index.row() + 1),
                                   value.value<sqlite::Value>(), editable, this);
    viewer->setAttribute(Qt::WA_DeleteOnClose);
    if (editable) {
        // The cell, wherever sorting or added rows move it meanwhile.
        const QPersistentModelIndex cell(index);
        connect(viewer, &ValueViewer::valueAccepted, this,
                [this, cell](const sqlite::Value &v, bool typedText) {
                    if (!cell.isValid())
                        return;
                    if (typedText)
                        m_model->setData(cell, v.toString(), Qt::EditRole);
                    else
                        m_model->setValue(cell, v);
                    m_editToggle->setChecked(true);
                });
    }
    viewer->show();
    return viewer;
}

void ResultView::showContextMenu(QTableView *view, const QPoint &pos)
{
    const QModelIndex cell = gridIndex(view->indexAt(pos));
    if (!cell.isValid())
        return;
    QMenu menu(this);
    menu.addAction(m_model->isWritable(cell) ? tr("Edit Value…") : tr("View Value…"),
                   QKeySequence(Qt::SHIFT | Qt::Key_Return), this,
                   [this, cell] { viewValue(cell); })
        ->setEnabled(m_model->data(cell, ResultModel::RawValueRole).isValid());
    if (m_model->isWritable(cell))
        menu.addAction(tr("Set NULL"), this,
                       [this] { m_model->setNull(m_table->selectionModel()->selectedIndexes()); });
    if (auto *copy = view->findChild<QAction *>(QStringLiteral("copy")))
        menu.addAction(copy);
    if (std::ranges::any_of(m_contextActions, &QAction::isVisible))
        menu.addSeparator();
    menu.addActions(m_contextActions);
    menu.exec(view->viewport()->mapToGlobal(pos));
}

int ResultView::columnIndex(const QString &name) const
{
    const sqlite::RowStore &rows = m_model->rows();
    for (int c = 0; c < rows.columnCount(); ++c) {
        if (rows.column(c).name == name)
            return c;
    }
    return -1;
}

void ResultView::setNote(const QString &text, bool error)
{
    m_note->setText(text);
    m_note->setStyleSheet(error ? QStringLiteral("color: #c62828;") : QString());
    m_note->setVisible(!text.isEmpty());
}

QString ResultView::note() const
{
    return m_note->text();
}

ResultView::Layout ResultView::layout() const
{
    Layout layout;
    const sqlite::RowStore &rows = m_model->rows();
    for (int c = 0; c < rows.columnCount(); ++c) {
        layout.columns << rows.column(c).name;
        layout.widths << m_table->columnWidth(c);
    }
    layout.horizontal = m_table->horizontalScrollBar()->value();
    layout.vertical = m_table->verticalScrollBar()->value();
    if (const int key = columnIndex(m_keyColumn); key >= 0) {
        for (const int row : selectedRows())
            layout.selectedKeys.insert(rows.value(row, key).toByteArray());
    }
    return layout;
}

void ResultView::restore(const Layout &layout)
{
    const sqlite::RowStore &rows = m_model->rows();
    if (layout.columns.isEmpty() || layout.columns.size() != rows.columnCount())
        return;
    for (int c = 0; c < rows.columnCount(); ++c) {
        if (rows.column(c).name != layout.columns[c])
            return; // Other columns: sized for what they hold.
    }
    m_sized = true;
    for (int c = 0; c < rows.columnCount(); ++c)
        m_table->setColumnWidth(c, layout.widths[c]);

    if (const int key = columnIndex(m_keyColumn); key >= 0 && !layout.selectedKeys.isEmpty()) {
        QItemSelection selection;
        for (int row = 0; row < m_model->rowCount(); ++row) {
            const int source = m_model->sourceRow(row);
            if (!rows.isNull(source, key)
                && layout.selectedKeys.contains(rows.value(source, key).toByteArray()))
                selection.select(m_model->index(row, 0), m_model->index(row, 0));
        }
        m_table->selectionModel()->select(
            selection, QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
    }
    m_table->horizontalScrollBar()->setValue(layout.horizontal);
    m_table->verticalScrollBar()->setValue(layout.vertical);
}

bool ResultView::showsRows() const
{
    return m_model->hasColumns() && m_stack->currentWidget() != m_message;
}

void ResultView::append(const sqlite::Result &result)
{
    m_model->append(result);
}

void ResultView::setViewMode(ViewMode mode)
{
    if (m_mode == mode)
        return;
    m_mode = mode;
    if (m_modeTabs->currentIndex() != int(mode))
        m_modeTabs->setCurrentIndex(int(mode));
    updateView();
}

void ResultView::updateView(bool force)
{
    if (!force && m_stack->currentWidget() == m_message && !m_model->hasColumns())
        return; // A message, an error or nothing run yet: no rows to show.

    switch (m_mode) {
    case ViewMode::Grid:
        m_stack->setCurrentWidget(m_table);
        return;
    case ViewMode::Text:
        if (m_textStale) {
            // Rendering a huge result would block; export writes them all.
            catalog::ExportOptions options = exportOptions(catalog::ExportFormat::Text);
            options.maxRows = MaxTextRows;
            options.rows = m_model->order();
            m_text->setText(catalog::exportRows(m_model->rows(), options));
            m_textStale = false;
        }
        m_stack->setCurrentWidget(m_text);
        return;
    case ViewMode::Record:
        updateRecord();
        m_stack->setCurrentWidget(m_record->parentWidget());
        return;
    }
}

void ResultView::updateRecord()
{
    const QModelIndex current = m_table->currentIndex();
    const int row = current.isValid() ? current.row() : (m_model->rowCount() > 0 ? 0 : -1);
    if (row != m_recordModel->row())
        m_recordModel->setRow(row);
    m_record->resizeColumnToContents(0);
    m_previousRow->setEnabled(row > 0);
    m_nextRow->setEnabled(row >= 0 && row + 1 < m_model->rowCount());
    m_recordLabel->setText(row < 0 ? QString()
                                   : tr("Row %1 of %2").arg(row + 1).arg(m_model->rowCount()));
}

void ResultView::stepRecord(int by)
{
    const int row = m_recordModel->row() + by;
    if (row < 0 || row >= m_model->rowCount())
        return;
    // Through the grid, so both views stay on the same row.
    m_table->setCurrentIndex(m_model->index(row, std::max(0, m_table->currentIndex().column())));
    updateRecord();
}

catalog::ExportOptions ResultView::exportOptions(catalog::ExportFormat format) const
{
    catalog::ExportOptions options;
    options.format = format;
    const catalog::EditTarget &target = m_model->editTarget();
    if (!target.table.isEmpty()) {
        options.table = target.schema.isEmpty() ? target.table
                                                : target.schema + QLatin1Char('.') + target.table;
    }
    return options;
}

std::vector<int> ResultView::selectedRows() const
{
    std::vector<int> rows;
    for (const QModelIndex &index : m_table->selectionModel()->selectedIndexes()) {
        if (std::ranges::find(rows, index.row()) == rows.end())
            rows.push_back(index.row());
    }
    std::ranges::sort(rows);
    for (int &row : rows)
        row = m_model->sourceRow(row);
    return rows;
}

void ResultView::setSortable(bool sortable)
{
    QHeaderView *header = m_table->horizontalHeader();
    header->setSortIndicatorClearable(true);
    // Not sorted at first: the query's ORDER BY is usually the point.
    header->setSortIndicator(-1, Qt::AscendingOrder);
    m_table->setSortingEnabled(sortable);
}

void ResultView::setAllRowsFetched(bool all)
{
    m_allFetched = all;
}

bool ResultView::exportTo(const catalog::ExportOptions &options, const QString &path,
                          QString *error) const
{
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        if (error)
            *error = file.errorString();
        return false;
    }
    QTextStream out(&file);
    catalog::exportRows(m_model->rows(), options, out);
    out.flush();
    if (!file.commit()) {
        if (error)
            *error = file.errorString();
        return false;
    }
    return true;
}

void ResultView::exportWithDialog()
{
    if (!m_model->hasColumns()) {
        QMessageBox::information(this, tr("Export"), tr("There are no rows to export."));
        return;
    }
    ExportDialog::Counts counts;
    counts.fetched = m_model->rowCount();
    counts.selected = int(selectedRows().size());
    counts.truncated = !m_allFetched;
    ExportDialog dialog(counts, exportOptions(catalog::ExportFormat::SqlInsert).table, this);
    if (dialog.exec() != QDialog::Accepted)
        return;

    catalog::ExportOptions options = dialog.options();
    if (dialog.scope() == ExportDialog::Scope::Selected)
        options.rows = selectedRows();
    const bool rerun = dialog.scope() == ExportDialog::Scope::All && !m_allFetched;
    if (options.rows.empty() && !rerun)
        options.rows = m_model->order(); // What was fetched, as shown.

    // Every row, but some are still on the server: whoever ran the query
    // runs it again and streams what comes back.
    if (rerun) {
        if (dialog.toClipboard()) {
            QMessageBox::information(this, tr("Export"),
                                     tr("Only the rows that were fetched can be copied to the "
                                        "clipboard. Save them to a file instead."));
            return;
        }
        Q_EMIT exportAllRequested(options, dialog.path());
        return;
    }

    if (dialog.toClipboard()) {
        QGuiApplication::clipboard()->setText(catalog::exportRows(m_model->rows(), options));
        return;
    }
    if (QString error; !exportTo(options, dialog.path(), &error)) {
        QMessageBox::warning(this, tr("Export"),
                             tr("Could not write %1: %2").arg(dialog.path(), error));
    }
}

void ResultView::copyAs(catalog::ExportFormat format)
{
    if (!m_model->hasColumns())
        return;
    catalog::ExportOptions options = exportOptions(format);
    // What is selected is what is meant; with no selection, everything, as shown.
    options.rows = selectedRows();
    if (options.rows.empty())
        options.rows = m_model->order();
    QGuiApplication::clipboard()->setText(catalog::exportRows(m_model->rows(), options));
}

void ResultView::sizeColumns()
{
    m_table->resizeColumnsToContents();
    for (int column = 0; column < m_model->columnCount(); ++column) {
        // Contents only up to a point: a JSON document or a query text would
        // otherwise push every other column off the view. The header's own
        // text always fits, though, since a column nobody can name is of no use.
        const int head = std::min(headerWidth(column), MaxHeaderWidth);
        const int contents = std::min(m_table->columnWidth(column), MaxContentWidth);
        m_table->setColumnWidth(column, std::max(head, contents));
    }
}

int ResultView::headerWidth(int column) const
{
    QHeaderView *header = m_table->horizontalHeader();
    // sectionSizeHint() would include the data; this is the label alone.
    QStyleOptionHeader option;
    option.initFrom(header);
    option.section = column;
    option.orientation = Qt::Horizontal;
    option.text = m_model->headerData(column, Qt::Horizontal).toString();
    const QSize text = header->fontMetrics().size(0, option.text);
    return header->style()
        ->sizeFromContents(QStyle::CT_HeaderSection, &option, text, header)
        .width();
}

void ResultView::stop()
{
    if (m_running && m_runner && m_job)
        m_runner->cancel(m_job);
}

void ResultView::finish(const QString &status)
{
    m_running = false;
    m_stop->hide();
    updateView(); // The text view renders what arrived.
    m_rerunAction->setEnabled(bool(m_rerun));
    m_status->setText(status);
    Q_EMIT finished();
}

void ResultView::showError(const QString &message)
{
    m_stop->hide();
    setMessage(message, true);
}

void ResultView::showMessage(const QString &text)
{
    setMessage(text, false);
}

void ResultView::setRerun(std::function<void()> rerun)
{
    m_rerun = std::move(rerun);
    m_rerunAction->setEnabled(bool(m_rerun) && !m_running);
}

void ResultView::setMessage(const QString &text, bool error)
{
    m_message->setText(text);
    m_message->setStyleSheet(error ? QStringLiteral("color: #c62828;") : QString());
    m_stack->setCurrentWidget(m_message);
}

} // namespace sqliteman
