// SPDX-FileCopyrightText: 2007-2026 Petr Vanek
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "WorkspacePage.h"

#include <QPointer>

class QLabel;
class QTableWidget;

namespace sqliteman {

class Session;

// The PRAGMAs of a database, what each means, and the value each has; a
// value is changed in place, and read back to show what SQLite made of it.
class PragmaPage : public WorkspacePage
{
    Q_OBJECT

public:
    enum Column { Name, Value, Description, ColumnCount };

    PragmaPage(Session *session, const QString &schema, QWidget *parent = nullptr);

    QString title() const override;
    QIcon icon() const override;

    Session *session() const { return m_session; }
    QString schema() const { return m_schema; }
    QTableWidget *table() const { return m_table; }
    // The value shown for a pragma, as SQLite reported it.
    QString value(const QString &pragma) const;
    // Sets a pragma, then reads them all again.
    void setValue(const QString &pragma, const QString &value);
    void refresh();

Q_SIGNALS:
    void loaded();

private:
    QPointer<Session> m_session;
    QString m_schema;
    QTableWidget *m_table = nullptr;
    QLabel *m_status = nullptr;
    bool m_filling = false;
};

} // namespace sqliteman
