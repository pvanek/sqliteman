// SPDX-FileCopyrightText: 2026 Petr Vanek
// SPDX-License-Identifier: GPL-3.0-or-later

#include "ResultModel.h"

#include "Settings.h"

#include <QCollator>
#include <QColor>
#include <QFont>
#include <QGuiApplication>
#include <QLocale>
#include <QPalette>

#include <algorithm>
#include <map>
#include <numeric>

namespace sqliteman {

using sqlite::ValueType;

namespace {

// Longer values are cut short in cells; the tooltip has more.
constexpr int MaxCellLength = 500;

// The start of a value, at most length UTF-16 units and an ellipsis when
// there is more. Only that start is decoded, and no character is split.
QString shortened(const QByteArray &utf8, qsizetype length)
{
    // A character takes four bytes at most, so these decode to enough.
    qsizetype bytes = std::min(utf8.size(), 4 * length);
    while (bytes > 0 && bytes < utf8.size() && (uchar(utf8[bytes]) & 0xC0) == 0x80)
        --bytes; // A continuation byte: back to where the character starts.
    QString text = QString::fromUtf8(utf8.constData(), bytes);
    if (text.size() > length)
        text.truncate(text.at(length).isLowSurrogate() ? length - 1 : length);
    else if (bytes == utf8.size())
        return text;
    return text + QChar(0x2026);
}

bool isDark()
{
    return QGuiApplication::palette().color(QPalette::Base).lightness() < 128;
}

QString sizeText(qsizetype bytes)
{
    return QLocale().formattedDataSize(bytes, 1, QLocale::DataSizeTraditionalFormat);
}

} // namespace

void ResultModel::clear()
{
    beginResetModel();
    m_rows.clear();
    m_numeric.clear();
    m_target = {};
    m_edits.clear();
    m_deleted.clear();
    m_added.clear();
    m_order.clear();
    endResetModel();
    Q_EMIT changesChanged();
    Q_EMIT editTargetChanged();
}

void ResultModel::append(const sqlite::Result &result)
{
    if (result.isNull() || result.columnCount() == 0)
        return;
    if (!m_rows.hasColumns()) {
        beginResetModel();
        m_rows.append(result);
        // SQLite types values, not columns: a column of numbers is one whose
        // first rows hold only numbers.
        m_numeric.assign(std::size_t(m_rows.columnCount()), false);
        for (int c = 0; c < m_rows.columnCount(); ++c) {
            bool any = false;
            bool all = true;
            for (int row = 0; row < std::min(m_rows.rowCount(), 100) && all; ++row) {
                const ValueType t = m_rows.type(row, c);
                if (t == ValueType::Null)
                    continue;
                any = true;
                all = t == ValueType::Integer || t == ValueType::Real;
            }
            m_numeric[std::size_t(c)] = any && all;
        }
        endResetModel();
        applySort();
        return;
    }
    if (result.rowCount() == 0)
        return;
    // Fetched rows go before rows the user added; sorted, they go at the end
    // first, then to their places.
    const int first = m_rows.rowCount();
    beginInsertRows({}, first, first + result.rowCount() - 1);
    m_rows.append(result);
    if (!m_order.empty()) {
        for (int row = first; row < m_rows.rowCount(); ++row)
            m_order.push_back(row);
    }
    endInsertRows();
    applySort();
}

void ResultModel::setEditTarget(const catalog::EditTarget &target)
{
    // Edits are kept by a row's place; editable rows stay where they came.
    if (target.editable()) {
        m_sortColumn = -1;
        applySort();
    }
    m_target = target;
    if (rowCount() > 0 && columnCount() > 0)
        Q_EMIT dataChanged(index(0, 0), index(rowCount() - 1, columnCount() - 1));
    Q_EMIT editTargetChanged();
}

int ResultModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : m_rows.rowCount() + int(m_added.size());
}

int ResultModel::columnCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : m_rows.columnCount();
}

std::optional<ResultModel::Value> ResultModel::value(int row, int column) const
{
    if (isAdded(row))
        return m_added[std::size_t(row - m_rows.rowCount())][std::size_t(column)];
    const auto edit = m_edits.constFind({row, column});
    if (edit != m_edits.cend())
        return *edit;
    return m_rows.valueAt(sourceRow(row), column);
}

bool ResultModel::isEdited(int row, int column) const
{
    if (isAdded(row))
        return m_added[std::size_t(row - m_rows.rowCount())][std::size_t(column)].has_value();
    return m_edits.contains({row, column});
}

QVariant ResultModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid())
        return {};
    const int row = index.row();
    const int column = index.column();
    const std::optional<Value> v = value(row, column);
    const bool unset = !v.has_value(); // An added row's cell left to its default.
    const bool null = v && v->isNull();
    const bool blob = v && v->type() == ValueType::Blob;
    const config::Preferences &prefs = Settings::instance().preferences();

    switch (role) {
    case Qt::DisplayRole: {
        if (unset)
            return QStringLiteral("DEFAULT");
        if (null)
            return prefs.nullText;
        if (blob)
            return QStringLiteral("%1 (%2)").arg(prefs.blobText, sizeText(v->data().size()));
        QString text = shortened(v->data(), MaxCellLength);
        return text.replace(QLatin1Char('\n'), QChar(0x21B5));
    }
    case Qt::EditRole:
        return v && !null ? QVariant(v->toString()) : QVariant(QString());
    case ValueRole:
        if (unset || null)
            return {};
        return blob ? QString::fromLatin1(v->data().toHex()) : v->toString();
    case RawValueRole:
        return v ? QVariant::fromValue(*v) : QVariant();
    case Qt::ToolTipRole:
        if (unset)
            return {};
        if (null)
            return tr("NULL");
        if (blob)
            return tr("A blob of %n byte(s)", nullptr, int(v->data().size()));
        return QStringLiteral("%1\n(%2)")
            .arg(shortened(v->data(), 4 * MaxCellLength), sqlite::typeName(v->type()));
    case Qt::TextAlignmentRole:
        return m_numeric[std::size_t(column)] ? QVariant(Qt::AlignRight | Qt::AlignVCenter)
                                              : QVariant(Qt::AlignLeft | Qt::AlignVCenter);
    case Qt::ForegroundRole:
        return null || unset || blob
            ? QGuiApplication::palette().color(QPalette::Disabled, QPalette::Text)
            : QVariant();
    case Qt::BackgroundRole: {
        const bool dark = isDark();
        if (m_deleted.contains(row))
            return dark ? QColor(110, 40, 40) : QColor(255, 215, 215);
        if (isAdded(row))
            return dark ? QColor(40, 90, 50) : QColor(215, 245, 215);
        if (isEdited(row, column))
            return dark ? QColor(110, 95, 30) : QColor(255, 243, 190);
        if (null && prefs.highlightNull)
            return prefs.nullColor.isValid()
                ? prefs.nullColor
                : (dark ? QColor(255, 255, 255, 12) : QColor(0, 0, 0, 8));
        if (blob && prefs.highlightBlob)
            return prefs.blobColor.isValid()
                ? prefs.blobColor
                : (dark ? QColor(80, 110, 160, 40) : QColor(60, 110, 220, 18));
        return {};
    }
    case Qt::FontRole: {
        QFont font;
        font.setItalic(null || unset);
        font.setStrikeOut(m_deleted.contains(row));
        return font;
    }
    default:
        return {};
    }
}

QString ResultModel::copyText(QModelIndexList cells)
{
    if (cells.size() == 1)
        return cells.first().data(ValueRole).toString();
    std::sort(cells.begin(), cells.end()); // By row, then by column.
    // A ragged selection still lines up: every row has a field for every
    // column selected anywhere, empty where the cell was not selected.
    std::vector<int> columns;
    for (const QModelIndex &cell : std::as_const(cells))
        columns.push_back(cell.column());
    std::sort(columns.begin(), columns.end());
    columns.erase(std::unique(columns.begin(), columns.end()), columns.end());

    QString out;
    for (qsizetype i = 0; i < cells.size();) {
        const int row = cells[i].row();
        if (!out.isEmpty())
            out += QLatin1Char('\n');
        for (std::size_t c = 0; c < columns.size(); ++c) {
            if (c > 0)
                out += QLatin1Char('\t');
            if (i == cells.size() || cells[i].row() != row || cells[i].column() != columns[c])
                continue;
            QString value = cells[i++].data(ValueRole).toString();
            // Quoted the way spreadsheets read pasted text.
            if (value.contains(QLatin1Char('\t')) || value.contains(QLatin1Char('\n'))
                || value.contains(QLatin1Char('\r')) || value.contains(QLatin1Char('"'))) {
                value.replace(QLatin1Char('"'), QLatin1String("\"\""));
                value = QLatin1Char('"') + value + QLatin1Char('"');
            }
            out += value;
        }
    }
    return out;
}

QVariant ResultModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (orientation == Qt::Vertical) {
        if (role != Qt::DisplayRole)
            return {};
        return isAdded(section) ? QVariant(QStringLiteral("*")) : QVariant(section + 1);
    }
    if (!m_rows.hasColumns() || section >= m_rows.columnCount())
        return {};
    const sqlite::Column &column = m_rows.column(section);
    switch (role) {
    case Qt::DisplayRole:
        return column.name;
    case Qt::ToolTipRole: {
        // Issue #2: with aliases, the header alone does not say where a
        // column comes from.
        QString tip = column.name;
        if (!column.table.isEmpty())
            tip += QStringLiteral("\n%1.%2.%3").arg(column.database, column.table, column.origin);
        if (!column.declaredType.isEmpty())
            tip += QLatin1Char('\n') + column.declaredType;
        if (m_target.editable() && std::size_t(section) < m_target.columns.size()) {
            if (std::find(m_target.key.begin(), m_target.key.end(), section) != m_target.key.end())
                tip += QLatin1Char('\n') + tr("Identifies the row");
            else if (!m_target.writable(section))
                tip += QLatin1Char('\n') + tr("Read-only");
        }
        return tip;
    }
    case Qt::FontRole: {
        if (std::find(m_target.key.begin(), m_target.key.end(), section) == m_target.key.end())
            return {};
        QFont font;
        font.setUnderline(true);
        return font;
    }
    default:
        return {};
    }
}

bool ResultModel::isWritable(const QModelIndex &index) const
{
    return index.isValid() && m_target.writable(index.column()) && !m_deleted.contains(index.row());
}

Qt::ItemFlags ResultModel::flags(const QModelIndex &index) const
{
    Qt::ItemFlags f = QAbstractTableModel::flags(index);
    // A blob is not text to type over; the value editor takes it.
    if (isWritable(index)) {
        const std::optional<Value> v = value(index.row(), index.column());
        if (!v || v->type() != ValueType::Blob)
            f |= Qt::ItemIsEditable;
    }
    return f;
}

bool ResultModel::setData(const QModelIndex &index, const QVariant &v, int role)
{
    if (!isWritable(index))
        return false;
    if (role == RawValueRole && v.canConvert<Value>()) {
        store(index.row(), index.column(), v.value<Value>());
        return true;
    }
    if (role != Qt::EditRole || !(flags(index) & Qt::ItemIsEditable))
        return false;
    const catalog::Affinity affinity = std::size_t(index.column()) < m_target.affinity.size()
        ? m_target.affinity[std::size_t(index.column())]
        : catalog::Affinity::Blob;
    store(index.row(), index.column(), catalog::typedValue(v.toString(), affinity));
    return true;
}

bool ResultModel::setValue(const QModelIndex &cell, const Value &value)
{
    return setData(cell, QVariant::fromValue(value), RawValueRole);
}

void ResultModel::store(int row, int column, const Value &v)
{
    if (isAdded(row)) {
        m_added[std::size_t(row - m_rows.rowCount())][std::size_t(column)] = v;
    } else {
        if (v == m_rows.valueAt(row, column))
            m_edits.remove({row, column}); // Back to what it was.
        else
            m_edits.insert({row, column}, v);
    }
    const QModelIndex i = index(row, column);
    Q_EMIT dataChanged(i, i);
    Q_EMIT changesChanged();
}

void ResultModel::setNull(const QModelIndexList &cells)
{
    for (const QModelIndex &i : cells) {
        if (isWritable(i))
            store(i.row(), i.column(), Value());
    }
}

int ResultModel::addRow()
{
    const int row = rowCount();
    beginInsertRows({}, row, row);
    m_added.emplace_back(std::size_t(columnCount()));
    endInsertRows();
    Q_EMIT changesChanged();
    return row;
}

void ResultModel::toggleDeleted(const QList<int> &rows)
{
    // Added rows go away, from the last, so indexes stay valid.
    QList<int> sorted = rows;
    std::ranges::sort(sorted, std::greater<>());
    for (const int row : sorted) {
        if (isAdded(row)) {
            beginRemoveRows({}, row, row);
            m_added.erase(m_added.begin() + (row - m_rows.rowCount()));
            endRemoveRows();
        } else if (!m_deleted.remove(row)) {
            m_deleted.insert(row);
        }
        if (!isAdded(row) && row < rowCount())
            Q_EMIT dataChanged(index(row, 0), index(row, columnCount() - 1));
    }
    Q_EMIT changesChanged();
}

void ResultModel::sort(int column, Qt::SortOrder order)
{
    m_sortColumn = column;
    m_sortOrder = order;
    applySort();
}

void ResultModel::applySort()
{
    if (isEditable() || hasChanges())
        return;
    const int rows = m_rows.rowCount();
    const int column = m_sortColumn < columnCount() ? m_sortColumn : -1;
    if (column < 0 && m_order.empty())
        return; // Already as the rows came.

    std::vector<int> order;
    if (column >= 0) {
        // The keys once, not once per comparison. Numbers before text, text
        // before blobs, as SQLite's own ORDER BY has them.
        std::vector<int> classes(static_cast<std::size_t>(rows));
        std::vector<QString> texts(static_cast<std::size_t>(rows));
        std::vector<double> numbers(static_cast<std::size_t>(rows));
        for (int row = 0; row < rows; ++row) {
            switch (m_rows.type(row, column)) {
            case ValueType::Null:
                classes[std::size_t(row)] = 3;
                break;
            case ValueType::Integer:
            case ValueType::Real:
                numbers[std::size_t(row)] = QByteArray(m_rows.value(row, column)).toDouble();
                break;
            case ValueType::Text:
                classes[std::size_t(row)] = 1;
                texts[std::size_t(row)] = QString::fromUtf8(m_rows.value(row, column));
                break;
            case ValueType::Blob:
                classes[std::size_t(row)] = 2;
                texts[std::size_t(row)]
                    = QString::fromLatin1(m_rows.value(row, column).toByteArray().toHex());
                break;
            }
        }
        // The C locale, which a build host or a bare environment runs in,
        // compares code points and ignores numeric mode: "b10" before "B2".
        QCollator collator(QLocale().language() == QLocale::C
                               ? QLocale(QLocale::English, QLocale::UnitedStates)
                               : QLocale());
        collator.setNumericMode(true);
        collator.setCaseSensitivity(Qt::CaseInsensitive);
        const bool descending = m_sortOrder == Qt::DescendingOrder;

        order.resize(std::size_t(rows));
        std::iota(order.begin(), order.end(), 0);
        std::ranges::stable_sort(order, [&](int a, int b) {
            const int ca = classes[std::size_t(a)];
            const int cb = classes[std::size_t(b)];
            if (ca == 3 || cb == 3)
                return ca != 3 && cb == 3; // NULLs last.
            int c = ca - cb;
            if (c == 0 && ca == 0) {
                const double x = numbers[std::size_t(a)];
                const double y = numbers[std::size_t(b)];
                c = x < y ? -1 : x > y ? 1 : 0;
            } else if (c == 0) {
                c = collator.compare(texts[std::size_t(a)], texts[std::size_t(b)]);
            }
            return descending ? c > 0 : c < 0;
        });
    }

    Q_EMIT layoutAboutToBeChanged({}, QAbstractItemModel::VerticalSortHint);
    // Selection and the current cell stay on their rows.
    std::vector<int> placeOf(static_cast<std::size_t>(rows));
    for (int row = 0; row < rows; ++row)
        placeOf[std::size_t(order.empty() ? row : order[std::size_t(row)])] = row;
    const QModelIndexList before = persistentIndexList();
    QModelIndexList after;
    for (const QModelIndex &i : before) {
        const int source = sourceRow(i.row());
        after << (source < rows ? index(placeOf[std::size_t(source)], i.column()) : i);
    }
    m_order = std::move(order);
    changePersistentIndexList(before, after);
    Q_EMIT layoutChanged({}, QAbstractItemModel::VerticalSortHint);
}

bool ResultModel::hasChanges() const
{
    return !m_edits.isEmpty() || !m_deleted.isEmpty() || !m_added.empty();
}

std::vector<catalog::RowEdit> ResultModel::changes() const
{
    using Edit = catalog::RowEdit;
    auto keyOf = [&](int row) {
        std::vector<Value> key;
        for (const int column : m_target.key)
            key.push_back(m_rows.valueAt(row, column));
        return key;
    };

    std::vector<Edit> out;
    QList<int> deleted(m_deleted.begin(), m_deleted.end());
    std::ranges::sort(deleted);
    for (const int row : deleted)
        out.push_back({Edit::Kind::Delete, keyOf(row), {}});

    // Updates, one per row, columns in order.
    std::map<int, std::vector<std::pair<int, Value>>> updates;
    for (auto it = m_edits.cbegin(); it != m_edits.cend(); ++it) {
        if (!m_deleted.contains(it.key().first))
            updates[it.key().first].push_back({it.key().second, *it});
    }
    for (auto &[row, values] : updates) {
        std::ranges::sort(values, {}, &std::pair<int, Value>::first);
        out.push_back({Edit::Kind::Update, keyOf(row), values});
    }

    for (const auto &added : m_added) {
        Edit e {Edit::Kind::Insert, {}, {}};
        for (int column = 0; column < int(added.size()); ++column) {
            if (added[std::size_t(column)])
                e.values.push_back({column, *added[std::size_t(column)]});
        }
        out.push_back(std::move(e));
    }
    return out;
}

void ResultModel::discardChanges()
{
    beginResetModel();
    m_edits.clear();
    m_deleted.clear();
    m_added.clear();
    endResetModel();
    Q_EMIT changesChanged();
}

} // namespace sqliteman
