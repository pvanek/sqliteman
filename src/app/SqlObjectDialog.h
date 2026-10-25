// SPDX-FileCopyrightText: 2007-2026 Petr Vanek
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QDialog>
#include <QPointer>

#include <functional>
#include <memory>

namespace Ui {
class SqlObjectDialog;
}

namespace sqliteman {

class Session;
class SqlEditor;

// Makes a view or a trigger from SQL, or changes one: SQLite has no ALTER
// for either, so the old one is dropped and the new one made, together, so
// that a mistake leaves the old one as it was.
class SqlObjectDialog : public QDialog
{
    Q_OBJECT

public:
    enum class Kind { View, Trigger };

    // name is the object to change; empty makes a new one. table is what a
    // new trigger is on.
    SqlObjectDialog(Session *session, Kind kind, const QString &schema, const QString &name,
                    const QString &table, QWidget *parent = nullptr);
    ~SqlObjectDialog() override;

    bool isAltering() const { return !m_original.isEmpty(); }
    // A new view on this query.
    void setViewQuery(const QString &select);
    // A new trigger on the table, AFTER INSERT for a table, INSTEAD OF for a view.
    void setTriggerTemplate(bool onView);
    void setName(const QString &name);
    SqlEditor *editor() const;
    // What OK runs, without the DROP of the old one.
    QString statement() const;
    QString statusText() const;
    void apply(std::function<void(bool ok)> done = {});

private:
    void updateEnabled();

    std::unique_ptr<Ui::SqlObjectDialog> m_ui; // Layout in SqlObjectDialog.ui.
    QPointer<Session> m_session;
    Kind m_kind;
    QString m_schema;
    QString m_original; // The name of the object being changed.
    QString m_table;
};

} // namespace sqliteman
