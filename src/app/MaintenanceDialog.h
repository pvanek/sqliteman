// SPDX-FileCopyrightText: 2007-2026 Petr Vanek
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QDialog>
#include <QPointer>

#include <functional>
#include <memory>

namespace Ui {
class MaintenanceDialog;
}

class QPushButton;

namespace sqliteman {

class Session;

// Keeping a database in shape: statistics, indexes, free space, and checks
// for damage and broken references. What each did goes to the log below.
class MaintenanceDialog : public QDialog
{
    Q_OBJECT

public:
    MaintenanceDialog(Session *session, const QString &schema, QWidget *parent = nullptr);
    ~MaintenanceDialog() override;

    QString logText() const;
    // The buttons, for tests.
    QPushButton *button(const QString &name) const;
    // Runs SQL as the buttons do and logs what it returned; done follows.
    void run(const QString &sql, const QString &title, std::function<void()> done = {});

private:
    QStringList selectedTables() const;
    void forEachTable(const QString &statement, const QString &title);
    void setBusy(bool busy);

    std::unique_ptr<Ui::MaintenanceDialog> m_ui; // Layout in MaintenanceDialog.ui.
    QPointer<Session> m_session;
    QString m_schema;
};

} // namespace sqliteman
