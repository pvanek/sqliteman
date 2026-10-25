// SPDX-FileCopyrightText: 2026 Petr Vanek
// SPDX-License-Identifier: GPL-3.0-or-later

#include "CompletionPopup.h"

#include "Icons.h"

#include <QAbstractListModel>
#include <QApplication>
#include <QKeyEvent>
#include <QListView>
#include <QPainter>
#include <QScreen>
#include <QStyledItemDelegate>
#include <QVBoxLayout>

#include <algorithm>

namespace sqliteman {

using catalog::CompletionItem;
using Kind = CompletionItem::Kind;

class CompletionModel : public QAbstractListModel
{
public:
    using QAbstractListModel::QAbstractListModel;

    enum { DetailRole = Qt::UserRole + 1, PrefixRole };

    void setItems(std::vector<CompletionItem> items, const QString &prefix)
    {
        beginResetModel();
        m_items = std::move(items);
        m_prefix = prefix;
        endResetModel();
    }
    const CompletionItem &at(int row) const { return m_items[std::size_t(row)]; }

    int rowCount(const QModelIndex &parent = {}) const override
    {
        return parent.isValid() ? 0 : int(m_items.size());
    }

    QVariant data(const QModelIndex &index, int role) const override
    {
        const CompletionItem &item = m_items[std::size_t(index.row())];
        switch (role) {
        case Qt::DisplayRole:
            return item.label;
        case DetailRole:
            return item.detail;
        case PrefixRole:
            return m_prefix;
        case Qt::ToolTipRole:
            return item.detail;
        case Qt::DecorationRole:
            return icon(item.kind);
        default:
            return {};
        }
    }

private:
    static QIcon icon(Kind kind)
    {
        using K = catalog::ObjectKind;
        switch (kind) {
        case Kind::Keyword:
            return {};
        case Kind::Schema:
            return Icons::object(K::Database);
        case Kind::Table:
        case Kind::Alias:
            return Icons::object(K::Table);
        case Kind::View:
            return Icons::object(K::View);
        case Kind::Column:
            return Icons::object(K::Column);
        case Kind::Function:
            return Icons::object(K::Trigger);
        case Kind::Pragma:
            return Icons::system();
        case Kind::Snippet:
            return QIcon::fromTheme(QStringLiteral("insert-text"),
                                    QIcon::fromTheme(QStringLiteral("format-text-code")));
        }
        return {};
    }

    std::vector<CompletionItem> m_items;
    QString m_prefix;
};

namespace {

// Name on the left with the typed letters picked out, detail (like a
// column's type) dimmed on the right.
class Delegate : public QStyledItemDelegate
{
public:
    using QStyledItemDelegate::QStyledItemDelegate;

    void paint(QPainter *painter, const QStyleOptionViewItem &option,
               const QModelIndex &index) const override
    {
        const QString label = index.data(Qt::DisplayRole).toString();
        // Everything but the label, which is drawn letter by letter below.
        QStyleOptionViewItem itemOption(option);
        initStyleOption(&itemOption, index);
        itemOption.text.clear();
        const QWidget *widget = itemOption.widget;
        QStyle *style = widget ? widget->style() : QApplication::style();
        style->drawControl(QStyle::CE_ItemViewItem, &itemOption, painter, widget);
        drawLabel(painter, itemOption, style, widget, label,
                  catalog::fuzzyMatchPositions(index.data(CompletionModel::PrefixRole).toString(),
                                               label));

        const QString detail = index.data(CompletionModel::DetailRole).toString();
        if (detail.isEmpty())
            return;
        const int left = option.rect.left() + option.decorationSize.width() + 12
            + option.fontMetrics.horizontalAdvance(label) + 16;
        QRect rect = option.rect.adjusted(0, 0, -6, 0);
        rect.setLeft(left);
        if (rect.width() < 30)
            return;
        painter->save();
        QColor color = option.palette.color(
            option.state & QStyle::State_Selected ? QPalette::HighlightedText : QPalette::Text);
        color.setAlphaF(0.55f);
        painter->setPen(color);
        painter->drawText(rect, Qt::AlignRight | Qt::AlignVCenter,
                          option.fontMetrics.elidedText(detail, Qt::ElideLeft, rect.width()));
        painter->restore();
    }

private:
    // The label, with the matched letters in bold and the accent color.
    static void drawLabel(QPainter *painter, const QStyleOptionViewItem &option, QStyle *style,
                          const QWidget *widget, const QString &label,
                          const std::vector<qsizetype> &matched)
    {
        const QRect rect = style->subElementRect(QStyle::SE_ItemViewItemText, &option, widget);
        const bool selected = option.state & QStyle::State_Selected;
        const QColor plain
            = option.palette.color(selected ? QPalette::HighlightedText : QPalette::Text);
        // On the highlight bar only the weight can carry the difference.
        const QColor accent = selected ? plain : option.palette.color(QPalette::Link);
        QFont bold = option.font;
        bold.setBold(true);
        const QFontMetrics boldMetrics(bold);

        painter->save();
        int x = rect.left();
        for (qsizetype i = 0; i < label.size() && x < rect.right(); ++i) {
            const bool hit = std::ranges::find(matched, i) != matched.end();
            painter->setFont(hit ? bold : option.font);
            painter->setPen(hit ? accent : plain);
            const QString letter = label.mid(i, 1);
            painter->drawText(QRect(x, rect.top(), rect.right() - x, rect.height()),
                              Qt::AlignLeft | Qt::AlignVCenter, letter);
            x += (hit ? boldMetrics : option.fontMetrics).horizontalAdvance(letter);
        }
        painter->restore();
    }
};

} // namespace

CompletionPopup::CompletionPopup(QWidget *editor)
    : QFrame(editor, Qt::ToolTip | Qt::FramelessWindowHint), m_list(new QListView(this)),
      m_model(new CompletionModel(this))
{
    setAttribute(Qt::WA_ShowWithoutActivating);
    setFocusPolicy(Qt::NoFocus);
    setFrameShape(QFrame::StyledPanel);
    m_list->setModel(m_model);
    m_list->setItemDelegate(new Delegate(m_list));
    m_list->setFocusPolicy(Qt::NoFocus);
    m_list->setUniformItemSizes(true);
    m_list->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_list->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    connect(m_list, &QListView::clicked, this, [this] { accept(); });

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(1, 1, 1, 1);
    layout->addWidget(m_list);
}

void CompletionPopup::showItems(std::vector<CompletionItem> items, const QString &prefix,
                                const QPoint &globalPos)
{
    m_model->setItems(std::move(items), prefix);
    m_list->setCurrentIndex(m_model->index(0));

    const int rowHeight = std::max(m_list->sizeHintForRow(0), fontMetrics().height() + 4);
    const int rows = std::min(m_model->rowCount(), 12);
    QSize size(420, rows * rowHeight + 4);
    // Keep it on screen: above the cursor line if there is no room below.
    QPoint pos = globalPos;
    if (const QScreen *screen = QGuiApplication::screenAt(globalPos)) {
        const QRect available = screen->availableGeometry();
        if (pos.y() + size.height() > available.bottom())
            pos.ry() -= size.height() + fontMetrics().height() + 4;
        pos.setX(std::min(pos.x(), available.right() - size.width()));
    }
    resize(size);
    QFrame::move(pos);
    show();
    raise();
}

int CompletionPopup::count() const
{
    return m_model->rowCount();
}

const CompletionItem *CompletionPopup::current() const
{
    const QModelIndex index = m_list->currentIndex();
    return index.isValid() ? &m_model->at(index.row()) : nullptr;
}

bool CompletionPopup::handleKey(QKeyEvent *event)
{
    if (!isVisible() || event->modifiers() & (Qt::ControlModifier | Qt::AltModifier))
        return false;
    switch (event->key()) {
    case Qt::Key_Up:
        move(-1);
        return true;
    case Qt::Key_Down:
        move(1);
        return true;
    case Qt::Key_PageUp:
        move(-10);
        return true;
    case Qt::Key_PageDown:
        move(10);
        return true;
    case Qt::Key_Return:
    case Qt::Key_Enter:
    case Qt::Key_Tab:
        accept();
        return true;
    case Qt::Key_Escape:
        hide();
        return true;
    default:
        return false;
    }
}

void CompletionPopup::move(int rows)
{
    const int count = m_model->rowCount();
    if (count == 0)
        return;
    int row = m_list->currentIndex().row() + rows;
    if (std::abs(rows) == 1) // Arrows wrap around; pages stop at the ends.
        row = (row + count) % count;
    else
        row = std::clamp(row, 0, count - 1);
    m_list->setCurrentIndex(m_model->index(row));
}

void CompletionPopup::accept()
{
    const CompletionItem *item = current();
    hide();
    if (item)
        Q_EMIT accepted(*item);
}

} // namespace sqliteman
