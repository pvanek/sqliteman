// SPDX-FileCopyrightText: 2007-2026 Petr Vanek
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "catalog/Snapshot.h"
// Complete, not declared: moc registers the signals' Session * argument,
// and GCC warns when that sees Session incomplete before its definition.
#include "Session.h"

#include <QPointer>
#include <QWidget>

class QAction;
class QLineEdit;

namespace sqliteman {

class ResultView;

// The rows of a table or view, to look through and edit: the Data tab of
// its object page. A filter and an order narrow them down without writing
// a query; a table's rows are edited in place and saved together.
class TableData : public QWidget
{
    Q_OBJECT

public:
    TableData(Session *session, const QString &schema, const QString &name,
              catalog::ObjectKind kind, QWidget *parent = nullptr);

    // Before the page closes: asks to save the edits. False keeps it open.
    bool maybeClose();

    Session *session() const { return m_session; }
    QString schema() const { return m_schema; }
    QString name() const { return m_name; }
    ResultView *results() const { return m_view; }
    QLineEdit *filter() const { return m_filter; }
    QLineEdit *order() const { return m_order; }

    // The SELECT the page runs: the rowid first where it is what finds a
    // row, then every column, filtered and ordered.
    QByteArray query() const;
    // Runs it again, asking first if edits would be lost.
    void refresh();
    // Writes the edits back; confirm shows the SQL first.
    void saveChanges(bool confirm = true);

Q_SIGNALS:
    // The table's Populate and Import, which the browser's dialogs do.
    void populateRequested(sqliteman::Session *session, const QString &schema,
                           const QString &table);
    void importRequested(sqliteman::Session *session, const QString &schema, const QString &table);

private:
    void updateEditTarget();
    // Whether the table finds its rows by a rowid that is no column of its own.
    bool needsRowid() const;

    QPointer<Session> m_session;
    QString m_schema;
    QString m_name;
    catalog::ObjectKind m_kind;
    ResultView *m_view = nullptr;
    QLineEdit *m_filter = nullptr;
    QLineEdit *m_order = nullptr;
    QAction *m_empty = nullptr;
};

} // namespace sqliteman
