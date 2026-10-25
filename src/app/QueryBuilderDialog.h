// SPDX-FileCopyrightText: 2007-2026 Petr Vanek
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "catalog/QueryBuilder.h"
#include "catalog/Snapshot.h"

#include <QDialog>

#include <memory>

namespace Ui {
class QueryBuilderDialog;
}

namespace sqliteman {

// A SELECT put together from lists: a table, its columns, conditions, an
// order and a limit. OK puts it into the editor, to run or to change.
class QueryBuilderDialog : public QDialog
{
    Q_OBJECT

public:
    QueryBuilderDialog(const catalog::SnapshotPtr &snapshot, QWidget *parent = nullptr);
    ~QueryBuilderDialog() override;

    // schema.table, as the table list has it.
    void setTable(const QString &schema, const QString &table);
    void selectColumns(const QStringList &columns);
    void addTerm(const QString &column, catalog::QuerySpec::Relation relation,
                 const QString &value);
    catalog::QuerySpec spec() const;
    QString statement() const;

private:
    void tableChanged();
    void moveColumns(bool toSelected, bool all);
    void updatePreview();

    std::unique_ptr<Ui::QueryBuilderDialog> m_ui; // Layout in QueryBuilderDialog.ui.
    catalog::SnapshotPtr m_snapshot;
    QStringList m_columns; // Of the table chosen, in its order.
};

} // namespace sqliteman
