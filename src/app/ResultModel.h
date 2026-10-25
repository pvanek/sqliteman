// SPDX-FileCopyrightText: 2026 Petr Vanek
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "catalog/Editing.h"
#include "sqlite/RowStore.h"

#include <QAbstractTableModel>
#include <QHash>
#include <QSet>

#include <optional>
#include <vector>

namespace sqliteman {

// The rows of one query, which may arrive in chunks, and edits to them that
// are not saved yet. Rows added by the user come after the fetched ones.
class ResultModel : public QAbstractTableModel
{
    Q_OBJECT

public:
    using Value = sqlite::Value;

    enum {
        // The whole value as text, where the cell shows it shortened: a
        // blob in hex digits. Invalid for NULL and for the cells of added
        // rows left to their default.
        ValueRole = Qt::UserRole + 1,
        // The value itself, a sqlite::Value; setData() takes one too, for a
        // blob or a NULL from the value editor.
        RawValueRole,
    };

    using QAbstractTableModel::QAbstractTableModel;

    void clear();
    // Adds a result's rows. The first result sets the columns; later ones
    // are chunks of the same query and must have the same columns.
    void append(const sqlite::Result &result);
    void setResult(const sqlite::Result &result)
    {
        clear();
        append(result);
    }
    bool hasColumns() const { return m_rows.hasColumns(); }
    const sqlite::RowStore &rows() const { return m_rows; }

    // Editing. Only once the target is known, and only its writable columns.
    void setEditTarget(const catalog::EditTarget &target);
    const catalog::EditTarget &editTarget() const { return m_target; }
    bool isEditable() const { return m_target.editable(); }

    int addRow();
    // Marks fetched rows deleted, or deleted ones not deleted again; drops
    // added ones.
    void toggleDeleted(const QList<int> &rows);
    void setNull(const QModelIndexList &cells);
    // Sets a cell, as the value editor does; false if it is not editable.
    bool setValue(const QModelIndex &cell, const Value &value);
    bool hasChanges() const;
    std::vector<catalog::RowEdit> changes() const;
    void discardChanges();
    bool isDeleted(int row) const { return m_deleted.contains(row); }
    bool isWritable(const QModelIndex &cell) const;

    // Sorting, for results nobody edits: numbers by value, text naturally,
    // NULLs last either way. Column -1 is the order the rows came in. It is
    // kept for the next result, so a query run again comes back sorted the
    // same way.
    void sort(int column, Qt::SortOrder order = Qt::AscendingOrder) override;
    // Where a row on screen is in rows(); they differ once sorted.
    int sourceRow(int row) const
    {
        return row < int(m_order.size()) ? m_order[std::size_t(row)] : row;
    }
    const std::vector<int> &order() const { return m_order; }
    bool isAdded(int row) const { return row >= m_rows.rowCount(); }

    // The cells' whole values for the clipboard: one cell as it is, more as
    // rows of tab-separated values. Any model with ValueRole will do.
    static QString copyText(QModelIndexList cells);

    int rowCount(const QModelIndex &parent = {}) const override;
    int columnCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QVariant headerData(int section, Qt::Orientation orientation,
                        int role = Qt::DisplayRole) const override;
    Qt::ItemFlags flags(const QModelIndex &index) const override;
    bool setData(const QModelIndex &index, const QVariant &value, int role = Qt::EditRole) override;

Q_SIGNALS:
    void changesChanged();
    void editTargetChanged();

private:
    // A cell's value as shown: an edit, else what was fetched; nullopt for
    // an added row's cell left to its default.
    std::optional<Value> value(int row, int column) const;
    bool isEdited(int row, int column) const;
    void store(int row, int column, const Value &v);
    // Puts the rows in the order asked for, moving what points at them along.
    void applySort();

    sqlite::RowStore m_rows;
    std::vector<bool> m_numeric; // Per column: right-align.
    catalog::EditTarget m_target;
    QHash<std::pair<int, int>, Value> m_edits; // Of fetched rows.
    QSet<int> m_deleted;
    std::vector<std::vector<std::optional<Value>>> m_added; // Unset cells take the default.
    std::vector<int> m_order; // Row on screen -> row in m_rows; empty: the same.
    int m_sortColumn = -1;
    Qt::SortOrder m_sortOrder = Qt::AscendingOrder;
};

} // namespace sqliteman
