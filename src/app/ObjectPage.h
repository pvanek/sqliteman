// SPDX-FileCopyrightText: 2026 Petr Vanek
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "catalog/Details.h"
#include "WorkspacePage.h"
// Complete, not declared: moc registers the signals' Session * argument,
// and GCC warns when that sees Session incomplete before its definition.
#include "Session.h"

#include <QPointer>

class QAction;
class QLabel;
class QTabWidget;
class QTableWidget;

namespace sqliteman {

class ErdView;
class SqlEditor;
class TableData;

// Everything about one table, view, index or trigger, or about a database:
// a tab each for a table's rows, its SQL, columns, indexes, foreign keys,
// triggers and its diagram; for a database, what the file is, what is in
// it, its schema and the diagram of its tables.
class ObjectPage : public WorkspacePage
{
    Q_OBJECT

public:
    // An object of schema; with an empty name, the database schema itself.
    ObjectPage(Session *session, const QString &schema, const QString &name,
               QWidget *parent = nullptr);

    QString title() const override;
    QIcon icon() const override;
    bool maybeClose() override;

    Session *session() const { return m_session; }
    QString schema() const { return m_schema; }
    QString name() const { return m_name; }
    bool isDatabase() const { return m_name.isEmpty(); }
    const catalog::ObjectDetails &details() const { return m_details; }
    const catalog::DatabaseDetails &databaseDetails() const { return m_database; }
    QTabWidget *tabs() const { return m_tabs; }
    QLabel *summary() const { return m_summary; }
    SqlEditor *sql() const { return m_sql; }
    QTableWidget *columns() const { return m_columns; }
    QTableWidget *indexes() const { return m_indexes; }
    QTableWidget *foreignKeys() const { return m_foreignKeys; }
    QTableWidget *properties() const { return m_properties; }
    QTableWidget *objects() const { return m_objects; }
    // The rows of a table or view; null for everything else.
    TableData *data() const { return m_data; }
    // The diagram of a table and its neighbours, or of a database's tables;
    // null for what has no foreign keys.
    ErdView *diagram() const { return m_diagram; }

    // Brings the tab forward.
    void showData();
    void showDiagram();
    // Reads it all again, but for the rows, which have their own refresh.
    void refresh();

Q_SIGNALS:
    void loaded();
    // Another object was double-clicked: a table in the diagram, or an
    // object in a database's list.
    void objectRequested(sqliteman::Session *session, const QString &schema, const QString &name);

private:
    void build();
    void showObject(const catalog::ObjectDetails &details);
    void showDatabase(const catalog::DatabaseDetails &details);
    void countRows();
    void loadDiagram();
    void exportDiagram(const QString &filter);
    void updateActions();

    QPointer<Session> m_session;
    QString m_schema;
    QString m_name;
    // What the snapshot said it is when the page was opened; it decides the
    // tabs. An index or trigger is not looked up, so it reads as an index.
    catalog::ObjectKind m_kind = catalog::ObjectKind::Index;
    catalog::ObjectDetails m_details;
    catalog::DatabaseDetails m_database;
    QLabel *m_summary = nullptr;
    QTabWidget *m_tabs = nullptr;
    TableData *m_data = nullptr;
    SqlEditor *m_sql = nullptr;
    QTableWidget *m_columns = nullptr;
    QTableWidget *m_indexes = nullptr;
    QTableWidget *m_foreignKeys = nullptr;
    QTableWidget *m_triggers = nullptr;
    QTableWidget *m_properties = nullptr;
    QTableWidget *m_objects = nullptr;
    QWidget *m_diagramTab = nullptr; // The diagram with its own little toolbar.
    ErdView *m_diagram = nullptr;
    bool m_diagramLoaded = false; // Laid out when its tab is first opened.
    QAction *m_refresh = nullptr;
    QAction *m_count = nullptr;
};

} // namespace sqliteman
