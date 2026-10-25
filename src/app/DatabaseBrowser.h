// SPDX-FileCopyrightText: 2007-2026 Petr Vanek
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "catalog/Snapshot.h"
// Complete, not declared: moc registers the signals' Session * argument,
// and GCC warns when that sees Session incomplete before its definition.
#include "Session.h"

#include <QSet>
#include <QWidget>

#include <functional>
#include <vector>

class QAction;
class QLineEdit;
class QMenu;
class QStandardItem;
class QStandardItemModel;
class QSortFilterProxyModel;
class QTreeView;

namespace sqliteman {

// The open databases and what is in them: for each file its databases
// (main, temp and the attached ones), their tables, views and SQLite's own
// tables, and a table's columns, indexes and triggers. Opens and closes the
// files, and does what the context menu offers on each, with the dialogs it
// takes.
class DatabaseBrowser : public QWidget
{
    Q_OBJECT

public:
    // What a node of the tree is.
    enum class Node { File, Schema, Folder, Object, Column };
    Q_ENUM(Node)
    enum class Folder { Tables, Views, System, Columns, Indexes, Triggers };
    Q_ENUM(Folder)

    enum Role {
        NodeRole = Qt::UserRole + 1,
        KindRole, // catalog::ObjectKind of an Object.
        FolderRole,
        SessionRole, // The Session, as a QObject *.
        SchemaRole,
        NameRole,
        TableRole, // What an index, trigger or column belongs to.
        SystemRole, // SQLite's own.
    };

    explicit DatabaseBrowser(QWidget *parent = nullptr);
    ~DatabaseBrowser() override;

    QTreeView *view() const { return m_view; }
    QStandardItemModel *model() const { return m_model; }
    QLineEdit *filter() const { return m_filter; }
    QAction *openAction() const { return m_open; }
    QAction *newAction() const { return m_new; }
    QAction *closeAction() const { return m_close; }
    QAction *refreshAction() const { return m_refresh; }

    // Opens a database file, or shows it if it is open already; null when
    // the user said no or it could not be opened, which is reported.
    Session *openDatabase(const QString &path, bool create = false, bool readOnly = false);
    // Closes it, asking first about an open transaction; false if the user
    // kept it open.
    bool closeDatabase(Session *session);
    bool closeAll();
    std::vector<Session *> sessions() const;
    // The session of the selected node, and its schema; null if none.
    Session *currentSession(QString *schema = nullptr) const;
    // The node of an object, or of a schema with an empty name.
    QModelIndex indexOf(const Session *session, const QString &schema,
                        const QString &name = {}) const;

    // What the context menu of a node offers; tests look at it without
    // opening the menu.
    void fillContextMenu(QMenu &menu, const QModelIndex &index);
    // How to ask; tests answer instead of a message box.
    void setConfirm(std::function<bool(const QString &question)> confirm)
    {
        m_confirm = std::move(confirm);
    }

    // The actions of the menus, for the main window's Database menu too.
    void createTable(Session *session, const QString &schema);
    void alterTable(Session *session, const QString &schema, const QString &table);
    void renameTable(Session *session, const QString &schema, const QString &table);
    void createView(Session *session, const QString &schema, const QString &select = {});
    void alterView(Session *session, const QString &schema, const QString &view);
    void createIndex(Session *session, const QString &schema, const QString &table);
    void createTrigger(Session *session, const QString &schema, const QString &table, bool onView);
    void alterTrigger(Session *session, const QString &schema, const QString &trigger);
    void drop(Session *session, catalog::ObjectKind kind, const QString &schema,
              const QString &name);
    void emptyTable(Session *session, const QString &schema, const QString &table);
    void reindex(Session *session, const QString &schema, const QString &name);
    void populate(Session *session, const QString &schema, const QString &table);
    void importCsv(Session *session, const QString &schema, const QString &table);
    void attach(Session *session);
    void detach(Session *session, const QString &schema);
    void loadExtension(Session *session);
    // The whole database as SQL, or its schema alone.
    void dump(Session *session, const QString &schema, bool schemaOnly);
    void maintenance(Session *session, const QString &schema);

Q_SIGNALS:
    // A database was opened or closed.
    void sessionsChanged();
    void databaseOpened(sqliteman::Session *session);
    // "New SQL Editor" on a database, maybe with a statement to start with.
    void editorRequested(sqliteman::Session *session, const QString &sql);
    // Everything about an object, a table's rows included; with an empty
    // name, about the database schema.
    void objectRequested(sqliteman::Session *session, const QString &schema, const QString &name);
    // The same, opened at its diagram: of a table, or of a database's tables.
    void diagramRequested(sqliteman::Session *session, const QString &schema, const QString &name);
    void pragmasRequested(sqliteman::Session *session, const QString &schema);
    // A line for the status bar.
    void message(const QString &text, bool error);

private:
    struct Target
    {
        Session *session = nullptr;
        Node node = Node::File;
        catalog::ObjectKind kind = catalog::ObjectKind::Table;
        Folder folder = Folder::Tables;
        QString schema;
        QString name;
        QString table;
        bool system = false;
    };
    Target targetOf(const QModelIndex &index) const;
    QStandardItem *fileItem(const Session *session) const;
    void addSession(Session *session);
    void rebuild(Session *session);
    void updateFileItem(Session *session);
    void showContextMenu(const QPoint &pos);
    void onActivated(const QModelIndex &index);
    void updateActions();
    bool confirm(const QString &title, const QString &question);
    // Runs SQL for a menu action and says how it went in the status bar.
    void runSql(Session *session, const QString &sql, const QString &done);
    QString pathOf(const QModelIndex &index) const;
    void saveExpanded(Session *session);
    void restoreExpanded(Session *session);
    void newDatabase();
    void openWithDialog();

    QStandardItemModel *m_model = nullptr;
    QSortFilterProxyModel *m_proxy = nullptr;
    QTreeView *m_view = nullptr;
    QLineEdit *m_filter = nullptr;
    std::vector<Session *> m_sessions;
    QSet<QString> m_expanded; // Of nodes being rebuilt, by pathOf().
    QAction *m_open = nullptr;
    QAction *m_new = nullptr;
    QAction *m_close = nullptr;
    QAction *m_refresh = nullptr;
    std::function<bool(const QString &)> m_confirm;
};

} // namespace sqliteman
