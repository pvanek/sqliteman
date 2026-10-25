// SPDX-FileCopyrightText: 2007-2026 Petr Vanek
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QDialog>
#include <QPointer>

#include <functional>
#include <memory>

namespace Ui {
class IndexDialog;
}

namespace sqliteman {

class Session;

// Makes an index on a table: which columns, in which order and direction,
// unique or not, and partial with a WHERE.
class IndexDialog : public QDialog
{
    Q_OBJECT

public:
    enum Column { Use, Name, Order, Collate, ColumnCount };

    IndexDialog(Session *session, const QString &schema, const QString &table,
                QWidget *parent = nullptr);
    ~IndexDialog() override;

    void setName(const QString &name);
    // Ticks a column; false if the table has none of that name.
    bool useColumn(const QString &column, bool descending = false);
    QString statement() const;
    QString statusText() const;
    // Runs it; tests call it instead of pressing OK.
    void apply(std::function<void(bool ok)> done = {});

private:
    void updatePreview();
    void move(int by);

    std::unique_ptr<Ui::IndexDialog> m_ui; // Layout in IndexDialog.ui.
    QPointer<Session> m_session;
    QString m_schema;
    QString m_tableName;
};

} // namespace sqliteman
