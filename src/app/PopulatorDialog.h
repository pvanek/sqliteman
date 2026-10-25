// SPDX-FileCopyrightText: 2007-2026 Petr Vanek
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "catalog/Populator.h"

#include <QDialog>
#include <QPointer>

#include <functional>
#include <memory>

namespace Ui {
class PopulatorDialog;
}

namespace sqliteman {

class Session;

// Fills a table with made-up rows: for each column, what to put in it.
class PopulatorDialog : public QDialog
{
    Q_OBJECT

public:
    enum Column { NameColumn, TypeColumn, ActionColumn, SizeColumn, ValueColumn, ColumnCount };

    PopulatorDialog(Session *session, const QString &schema, const QString &table,
                    QWidget *parent = nullptr);
    ~PopulatorDialog() override;

    bool isValid() const { return m_valid; }
    std::vector<catalog::populator::Column> columns() const;
    void setAction(int row, catalog::populator::Action action, const QString &value = {});
    void setCount(int count);
    QString statusText() const;
    // Runs it; tests call it instead of pressing OK.
    void apply(std::function<void(bool ok)> done = {});

private:
    void updateRow(int row);
    void updateEnabled();

    std::unique_ptr<Ui::PopulatorDialog> m_ui; // Layout in PopulatorDialog.ui.
    QPointer<Session> m_session;
    QString m_schema;
    QString m_tableName;
    std::vector<catalog::populator::Column> m_columns;
    bool m_valid = false;
    bool m_running = false;
};

} // namespace sqliteman
