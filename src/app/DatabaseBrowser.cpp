// SPDX-FileCopyrightText: 2007-2026 Petr Vanek
// SPDX-License-Identifier: GPL-3.0-or-later

#include "DatabaseBrowser.h"

#include "Icons.h"
#include "ImportDialog.h"
#include "IndexDialog.h"
#include "MaintenanceDialog.h"
#include "PopulatorDialog.h"
#include "Session.h"
#include "Settings.h"
#include "Shortcuts.h"
#include "SqlObjectDialog.h"
#include "TableDialog.h"
#include "catalog/Details.h"
#include "catalog/Dump.h"
#include "sql/Keywords.h"
#include "sqlite/Quote.h"

#include <QAction>
#include <QApplication>
#include <QClipboard>
#include <QEventLoop>
#include <QFileDialog>
#include <QFileInfo>
#include <QHeaderView>
#include <QInputDialog>
#include <QLineEdit>
#include <QMenu>
#include <QMessageBox>
#include <QMimeData>
#include <QPushButton>
#include <QSortFilterProxyModel>
#include <QStandardItemModel>
#include <QToolBar>
#include <QTreeView>
#include <QVBoxLayout>

namespace sqliteman {

using catalog::ObjectKind;

namespace {

const QString DirectoryKey = QStringLiteral("files/databaseDirectory");

QString databaseFilter()
{
    return QCoreApplication::translate("DatabaseBrowser",
                                       "SQLite databases (*.db *.sqlite *.sqlite3 *.db3 *.s3db);;"
                                       "All files (*)");
}

// Dragging a node into an editor drops its name, quoted as SQL needs it.
class BrowserItemModel : public QStandardItemModel
{
public:
    using QStandardItemModel::QStandardItemModel;

    QStringList mimeTypes() const override { return {QStringLiteral("text/plain")}; }

    QMimeData *mimeData(const QModelIndexList &indexes) const override
    {
        QStringList names;
        for (const QModelIndex &i : indexes) {
            const QString name = i.data(DatabaseBrowser::NameRole).toString();
            if (!name.isEmpty())
                names << sql::identifier(name);
        }
        if (names.isEmpty())
            return nullptr;
        auto *data = new QMimeData;
        data->setText(names.join(QLatin1String(", ")));
        return data;
    }
};

QStandardItem *item(const QIcon &icon, const QString &text, DatabaseBrowser::Node node)
{
    auto *i = new QStandardItem(icon, text);
    i->setEditable(false);
    i->setDragEnabled(false);
    i->setData(int(node), DatabaseBrowser::NodeRole);
    return i;
}

QString folderTitle(DatabaseBrowser::Folder folder)
{
    const auto tr
        = [](const char *text) { return QCoreApplication::translate("DatabaseBrowser", text); };
    switch (folder) {
    case DatabaseBrowser::Folder::Tables:
        return tr("Tables");
    case DatabaseBrowser::Folder::Views:
        return tr("Views");
    case DatabaseBrowser::Folder::System:
        return tr("System");
    case DatabaseBrowser::Folder::Columns:
        return tr("Columns");
    case DatabaseBrowser::Folder::Indexes:
        return tr("Indexes");
    case DatabaseBrowser::Folder::Triggers:
        return tr("Triggers");
    }
    return {};
}

} // namespace

DatabaseBrowser::DatabaseBrowser(QWidget *parent)
    : QWidget(parent), m_model(new BrowserItemModel(this)),
      m_proxy(new QSortFilterProxyModel(this)), m_view(new QTreeView(this)),
      m_filter(new QLineEdit(this))
{
    m_proxy->setSourceModel(m_model);
    m_proxy->setRecursiveFilteringEnabled(true);
    m_proxy->setFilterCaseSensitivity(Qt::CaseInsensitive);
    m_proxy->setFilterRole(NameRole);
    m_view->setModel(m_proxy);
    m_view->setHeaderHidden(true);
    m_view->setUniformRowHeights(true);
    m_view->setContextMenuPolicy(Qt::CustomContextMenu);
    m_view->setDragEnabled(true);
    m_view->setDragDropMode(QAbstractItemView::DragOnly);
    m_view->setSelectionMode(QAbstractItemView::ExtendedSelection);
    connect(m_view, &QWidget::customContextMenuRequested, this, &DatabaseBrowser::showContextMenu);
    connect(m_view, &QTreeView::activated, this, &DatabaseBrowser::onActivated);
    connect(m_view->selectionModel(), &QItemSelectionModel::currentChanged, this,
            &DatabaseBrowser::updateActions);

    m_filter->setPlaceholderText(tr("Filter by name"));
    m_filter->setClearButtonEnabled(true);
    connect(m_filter, &QLineEdit::textChanged, this, [this](const QString &text) {
        m_proxy->setFilterFixedString(text);
        if (!text.isEmpty())
            m_view->expandAll();
    });

    auto action = [this](const QIcon &icon, const QString &text, auto slot) {
        auto *a = new QAction(icon, text, this);
        connect(a, &QAction::triggered, this, slot);
        return a;
    };
    m_open = action(QIcon::fromTheme(QStringLiteral("document-open")), tr("&Open Database…"),
                    &DatabaseBrowser::openWithDialog);
    m_new = action(QIcon::fromTheme(QStringLiteral("document-new")), tr("&New Database…"),
                   &DatabaseBrowser::newDatabase);
    m_close = action(Icons::close(), tr("&Close Database"), [this] {
        if (Session *s = currentSession())
            closeDatabase(s);
    });
    m_refresh = action(Icons::refresh(), tr("&Refresh"), [this] {
        if (Session *s = currentSession())
            s->reloadSnapshot();
    });
    m_refresh->setShortcuts(Shortcuts::refresh());
    m_refresh->setShortcutContext(Qt::WidgetWithChildrenShortcut);
    addAction(m_refresh);

    auto *toolbar = new QToolBar(this);
    toolbar->setIconSize(QSize(16, 16));
    toolbar->addAction(m_open);
    toolbar->addAction(m_new);
    toolbar->addAction(m_close);
    toolbar->addAction(m_refresh);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    layout->addWidget(toolbar);
    layout->addWidget(m_filter);
    layout->addWidget(m_view);
    updateActions();
}

DatabaseBrowser::~DatabaseBrowser()
{
    for (Session *s : m_sessions) {
        s->disconnect(this);
        s->close();
    }
}

std::vector<Session *> DatabaseBrowser::sessions() const
{
    return m_sessions;
}

Session *DatabaseBrowser::openDatabase(const QString &path, bool create, bool readOnly)
{
    const QString absolute
        = path == QLatin1String(":memory:") ? path : QFileInfo(path).absoluteFilePath();
    for (Session *s : m_sessions) {
        if (s->path() == absolute && absolute != QLatin1String(":memory:")) {
            if (QStandardItem *file = fileItem(s))
                m_view->setCurrentIndex(m_proxy->mapFromSource(file->index()));
            if (!s->isOpen())
                s->open(create, readOnly);
            return s;
        }
    }
    auto *session = new Session(absolute, this);
    addSession(session);
    session->open(create, readOnly);
    return session;
}

void DatabaseBrowser::addSession(Session *session)
{
    m_sessions.push_back(session);
    QStandardItem *file = item(Icons::database(false), session->name(), Node::File);
    file->setData(QVariant::fromValue<QObject *>(session), SessionRole);
    file->setToolTip(session->path());
    m_model->appendRow(file);
    m_view->setCurrentIndex(m_proxy->mapFromSource(file->index()));

    connect(session, &Session::stateChanged, this, [this, session](Session::State state) {
        updateFileItem(session);
        if (state == Session::State::Failed) {
            const QString error = session->errorMessage();
            QMessageBox::warning(this, tr("Open Database"),
                                 tr("Could not open %1:\n%2").arg(session->path(), error));
            // Nothing to keep of a file that is no database.
            m_sessions.erase(std::ranges::find(m_sessions, session));
            if (QStandardItem *f = fileItem(session))
                m_model->removeRow(f->row());
            session->deleteLater();
            Q_EMIT sessionsChanged();
        } else if (state == Session::State::Open) {
            Q_EMIT databaseOpened(session);
            Q_EMIT sessionsChanged();
        }
        updateActions();
    });
    connect(session, &Session::snapshotChanged, this, [this, session] { rebuild(session); });
    connect(session, &Session::transactionChanged, this,
            [this, session] { updateFileItem(session); });
    connect(session, &Session::message, this, &DatabaseBrowser::message);
    Q_EMIT sessionsChanged();
}

bool DatabaseBrowser::closeDatabase(Session *session)
{
    if (!session)
        return true;
    if (session->inTransaction()) {
        QMessageBox box(QMessageBox::Question, tr("Close Database"),
                        tr("%1 has a transaction open. Commit it before closing, or roll it back?")
                            .arg(session->name()),
                        QMessageBox::NoButton, this);
        QAbstractButton *commit = box.addButton(tr("&Commit"), QMessageBox::AcceptRole);
        QAbstractButton *rollback = box.addButton(tr("&Roll Back"), QMessageBox::DestructiveRole);
        box.addButton(QMessageBox::Cancel);
        box.exec();
        if (box.clickedButton() == commit) {
            // Closing rolls back: commit first, and wait for it.
            bool committed = false;
            QEventLoop wait;
            session->runner()->run("COMMIT", &wait, [&](const sqlite::QueryOutcome &o) {
                committed = o.ok();
                if (!o.ok())
                    QMessageBox::warning(this, tr("Commit"), o.error);
                wait.quit();
            });
            wait.exec();
            if (!committed)
                return false;
        } else if (box.clickedButton() != rollback) {
            return false;
        }
    }
    session->disconnect(this);
    session->close();
    m_sessions.erase(std::ranges::find(m_sessions, session));
    if (QStandardItem *file = fileItem(session))
        m_model->removeRow(file->row());
    Q_EMIT sessionsChanged();
    session->deleteLater();
    updateActions();
    return true;
}

bool DatabaseBrowser::closeAll()
{
    const std::vector<Session *> all = m_sessions;
    for (Session *s : all) {
        if (!closeDatabase(s))
            return false;
    }
    return true;
}

QStandardItem *DatabaseBrowser::fileItem(const Session *session) const
{
    for (int row = 0; row < m_model->rowCount(); ++row) {
        QStandardItem *file = m_model->item(row);
        if (file->data(SessionRole).value<QObject *>() == session)
            return file;
    }
    return nullptr;
}

void DatabaseBrowser::updateFileItem(Session *session)
{
    QStandardItem *file = fileItem(session);
    if (!file)
        return;
    file->setIcon(session->inTransaction() ? Icons::transaction()
                                           : Icons::database(session->isOpen()));
    QString text = session->name();
    if (session->state() == Session::State::Opening)
        text = tr("%1 (opening…)").arg(text);
    else if (session->isOpen() && session->isReadOnly())
        text = tr("%1 (read-only)").arg(text);
    file->setText(text);
    file->setToolTip(session->inTransaction()
                         ? tr("%1\nIn a transaction: commit or roll it back").arg(session->path())
                         : session->path());
}

QString DatabaseBrowser::pathOf(const QModelIndex &index) const
{
    QStringList parts;
    for (QModelIndex i = index; i.isValid(); i = i.parent())
        parts.prepend(i.data(NodeRole).toString() + QLatin1Char(':') + i.data(NameRole).toString()
                      + QLatin1Char(':') + i.data(FolderRole).toString() + QLatin1Char(':')
                      + i.data(SchemaRole).toString());
    return parts.join(QLatin1Char('/'));
}

void DatabaseBrowser::saveExpanded(Session *session)
{
    QStandardItem *file = fileItem(session);
    if (!file)
        return;
    m_expanded.clear();
    std::function<void(const QModelIndex &)> walk = [&](const QModelIndex &source) {
        const QModelIndex shown = m_proxy->mapFromSource(source);
        if (shown.isValid() && m_view->isExpanded(shown))
            m_expanded.insert(pathOf(source));
        for (int row = 0; row < m_model->rowCount(source); ++row)
            walk(m_model->index(row, 0, source));
    };
    walk(file->index());
}

void DatabaseBrowser::restoreExpanded(Session *session)
{
    QStandardItem *file = fileItem(session);
    if (!file)
        return;
    std::function<void(const QModelIndex &)> walk = [&](const QModelIndex &source) {
        if (m_expanded.contains(pathOf(source)))
            m_view->setExpanded(m_proxy->mapFromSource(source), true);
        for (int row = 0; row < m_model->rowCount(source); ++row)
            walk(m_model->index(row, 0, source));
    };
    walk(file->index());
}

void DatabaseBrowser::rebuild(Session *session)
{
    QStandardItem *file = fileItem(session);
    const catalog::SnapshotPtr snapshot = session->snapshot();
    if (!file || !snapshot)
        return;
    const bool first = !file->hasChildren();
    saveExpanded(session);
    file->removeRows(0, file->rowCount());

    auto object = [&](const catalog::SchemaObject &o) {
        QStandardItem *i = item(Icons::object(o.kind), o.name, Node::Object);
        i->setData(int(o.kind), KindRole);
        i->setData(o.schema, SchemaRole);
        i->setData(o.name, NameRole);
        i->setData(o.table, TableRole);
        i->setData(o.system, SystemRole);
        i->setDragEnabled(true);
        if (!o.error.isEmpty()) {
            i->setIcon(Icons::error());
            i->setToolTip(o.error);
        } else if (!o.sql.isEmpty()) {
            i->setToolTip(o.sql.size() > 500 ? o.sql.left(500) + QChar(0x2026) : o.sql);
        }
        return i;
    };
    auto folder = [&](Folder f, const QString &schema, const QString &table, int count) {
        QStandardItem *i
            = item(Icons::folder(), QStringLiteral("%1 (%2)").arg(folderTitle(f)).arg(count),
                   Node::Folder);
        i->setData(int(f), FolderRole);
        i->setData(schema, SchemaRole);
        i->setData(table, TableRole);
        return i;
    };

    for (const catalog::DatabaseInfo &d : snapshot->databases) {
        // temp is there in every connection; shown once something is in it.
        if (d.name == QLatin1String("temp") && d.objects.empty())
            continue;
        QStandardItem *schema = item(Icons::object(ObjectKind::Database), d.name, Node::Schema);
        schema->setData(d.name, SchemaRole);
        schema->setToolTip(d.file.isEmpty() ? d.name : d.file);
        if (d.readOnly)
            schema->setText(tr("%1 (read-only)").arg(d.name));
        file->appendRow(schema);

        std::vector<const catalog::SchemaObject *> tables, views, system;
        for (const catalog::SchemaObject &o : d.objects) {
            if (o.kind != ObjectKind::Table && o.kind != ObjectKind::VirtualTable
                && o.kind != ObjectKind::View)
                continue;
            (o.system ? system : o.kind == ObjectKind::View ? views : tables).push_back(&o);
        }
        // Every database has a schema table, which sqlite_schema does not list.
        catalog::SchemaObject master;
        master.schema = d.name;
        master.name = d.name == QLatin1String("temp") ? QStringLiteral("sqlite_temp_schema")
                                                      : QStringLiteral("sqlite_schema");
        master.table = master.name;
        master.system = true;

        auto fill = [&](QStandardItem *parent, const catalog::SchemaObject &o) {
            QStandardItem *columns = folder(Folder::Columns, d.name, o.name, int(o.columns.size()));
            for (const catalog::ColumnInfo &c : o.columns) {
                QStandardItem *column = item(
                    c.primaryKey > 0 ? Icons::primaryKey() : Icons::object(ObjectKind::Column),
                    c.type.isEmpty() ? c.name : c.name + QLatin1String(" : ") + c.type,
                    Node::Column);
                column->setData(d.name, SchemaRole);
                column->setData(c.name, NameRole);
                column->setData(o.name, TableRole);
                column->setDragEnabled(true);
                QStringList notes;
                if (c.primaryKey > 0)
                    notes << tr("primary key");
                if (c.notNull)
                    notes << tr("not null");
                if (c.hasDefault)
                    notes << tr("default %1").arg(c.defaultValue);
                if (c.isGenerated())
                    notes << tr("generated");
                column->setToolTip(notes.join(QLatin1String(", ")));
                columns->appendRow(column);
            }
            parent->appendRow(columns);
            if (o.kind == ObjectKind::Table) {
                const auto indexes = snapshot->belongingTo(d.name, o.name, ObjectKind::Index);
                QStandardItem *indexFolder
                    = folder(Folder::Indexes, d.name, o.name, int(indexes.size()));
                for (const catalog::SchemaObject *index : indexes)
                    indexFolder->appendRow(object(*index));
                parent->appendRow(indexFolder);
            }
            if (o.kind == ObjectKind::Table || o.kind == ObjectKind::View) {
                const auto triggers = snapshot->belongingTo(d.name, o.name, ObjectKind::Trigger);
                QStandardItem *triggerFolder
                    = folder(Folder::Triggers, d.name, o.name, int(triggers.size()));
                for (const catalog::SchemaObject *trigger : triggers)
                    triggerFolder->appendRow(object(*trigger));
                parent->appendRow(triggerFolder);
            }
        };

        QStandardItem *tableFolder = folder(Folder::Tables, d.name, {}, int(tables.size()));
        for (const catalog::SchemaObject *t : tables) {
            QStandardItem *table = object(*t);
            fill(table, *t);
            tableFolder->appendRow(table);
        }
        schema->appendRow(tableFolder);
        QStandardItem *viewFolder = folder(Folder::Views, d.name, {}, int(views.size()));
        for (const catalog::SchemaObject *v : views) {
            QStandardItem *view = object(*v);
            fill(view, *v);
            viewFolder->appendRow(view);
        }
        schema->appendRow(viewFolder);
        QStandardItem *systemFolder = folder(Folder::System, d.name, {}, int(system.size()) + 1);
        systemFolder->setIcon(Icons::system());
        systemFolder->appendRow(object(master));
        for (const catalog::SchemaObject *s : system)
            systemFolder->appendRow(object(*s));
        schema->appendRow(systemFolder);
    }

    // At first the main database's tables are in sight; later what was open.
    if (first) {
        m_view->setExpanded(m_proxy->mapFromSource(file->index()), true);
        if (file->hasChildren()) {
            QStandardItem *main = file->child(0);
            m_view->setExpanded(m_proxy->mapFromSource(main->index()), true);
            if (main->hasChildren())
                m_view->setExpanded(m_proxy->mapFromSource(main->child(0)->index()), true);
        }
    } else {
        restoreExpanded(session);
    }
    updateFileItem(session);
}

DatabaseBrowser::Target DatabaseBrowser::targetOf(const QModelIndex &shown) const
{
    const QModelIndex index = shown.model() == m_proxy ? m_proxy->mapToSource(shown) : shown;
    Target t;
    if (!index.isValid())
        return t;
    QModelIndex top = index;
    while (top.parent().isValid())
        top = top.parent();
    t.session = qobject_cast<Session *>(top.data(SessionRole).value<QObject *>());
    t.node = Node(index.data(NodeRole).toInt());
    t.kind = ObjectKind(index.data(KindRole).toInt());
    t.folder = Folder(index.data(FolderRole).toInt());
    t.schema = index.data(SchemaRole).toString();
    t.name = index.data(NameRole).toString();
    t.table = index.data(TableRole).toString();
    t.system = index.data(SystemRole).toBool();
    if (t.node == Node::File)
        t.schema = QStringLiteral("main");
    return t;
}

Session *DatabaseBrowser::currentSession(QString *schema) const
{
    const Target t = targetOf(m_view->currentIndex());
    if (schema)
        *schema = t.schema.isEmpty() ? QStringLiteral("main") : t.schema;
    if (t.session)
        return t.session;
    return m_sessions.size() == 1 ? m_sessions.front() : nullptr;
}

QModelIndex DatabaseBrowser::indexOf(const Session *session, const QString &schema,
                                     const QString &name) const
{
    QStandardItem *file = fileItem(session);
    if (!file)
        return {};
    std::function<QModelIndex(const QModelIndex &)> find
        = [&](const QModelIndex &at) -> QModelIndex {
        const Node node = Node(at.data(NodeRole).toInt());
        if (at.data(SchemaRole).toString() == schema
            && ((name.isEmpty() && node == Node::Schema)
                || (!name.isEmpty() && node == Node::Object
                    && at.data(NameRole).toString() == name)))
            return m_proxy->mapFromSource(at);
        for (int row = 0; row < m_model->rowCount(at); ++row) {
            if (const QModelIndex found = find(m_model->index(row, 0, at)); found.isValid())
                return found;
        }
        return {};
    };
    return find(file->index());
}

void DatabaseBrowser::updateActions()
{
    const bool any = currentSession() != nullptr;
    m_close->setEnabled(any);
    m_refresh->setEnabled(any);
}

void DatabaseBrowser::onActivated(const QModelIndex &index)
{
    const Target t = targetOf(index);
    if (!t.session || !t.session->isOpen())
        return;
    // Files, databases and folders expand instead, and have Details in
    // their context menu.
    if (t.node == Node::Object)
        Q_EMIT objectRequested(t.session, t.schema, t.name);
}

void DatabaseBrowser::showContextMenu(const QPoint &pos)
{
    const QModelIndex index = m_view->indexAt(pos);
    QMenu menu(this);
    fillContextMenu(menu, index);
    if (!menu.isEmpty())
        menu.exec(m_view->viewport()->mapToGlobal(pos));
}

void DatabaseBrowser::fillContextMenu(QMenu &menu, const QModelIndex &index)
{
    const Target t = targetOf(index);
    if (!t.session) {
        menu.addAction(m_open);
        menu.addAction(m_new);
        return;
    }
    Session *s = t.session;
    const QString schema = t.schema;
    const bool open = s->isOpen();
    auto add = [&](const QIcon &icon, const QString &text, auto slot) {
        QAction *a = menu.addAction(icon, text, this, slot);
        a->setEnabled(open);
        return a;
    };
    auto copyName = [&menu, this, name = t.name] {
        menu.addAction(QIcon::fromTheme(QStringLiteral("edit-copy")), tr("Copy &Name"), this,
                       [name] { QGuiApplication::clipboard()->setText(sql::identifier(name)); });
    };
    const QIcon none;

    switch (t.node) {
    case Node::File:
    case Node::Schema:
        add(QIcon::fromTheme(QStringLiteral("dialog-information")), tr("&Details"),
            [this, s, schema] { Q_EMIT objectRequested(s, schema, QString()); });
        add(Icons::diagram(), tr("&Diagram"),
            [this, s, schema] { Q_EMIT diagramRequested(s, schema, QString()); });
        add(QIcon::fromTheme(QStringLiteral("document-new")), tr("New SQL &Editor"),
            [this, s] { Q_EMIT editorRequested(s, QString()); });
        menu.addSeparator();
        add(Icons::object(ObjectKind::Table), tr("Create &Table…"),
            [this, s, schema] { createTable(s, schema); });
        add(Icons::object(ObjectKind::View), tr("Create &View…"),
            [this, s, schema] { createView(s, schema); });
        add(Icons::importData(), tr("&Import CSV into a New Table…"),
            [this, s, schema] { importCsv(s, schema, QString()); });
        menu.addSeparator();
        add(Icons::exportData(), tr("&Dump Database…"),
            [this, s, schema] { dump(s, schema, false); });
        add(none, tr("Export &Schema…"), [this, s, schema] { dump(s, schema, true); });
        add(Icons::system(), tr("&Pragmas"),
            [this, s, schema] { Q_EMIT pragmasRequested(s, schema); });
        add(none, tr("&Maintenance…"), [this, s, schema] { maintenance(s, schema); });
        menu.addSeparator();
        if (t.node == Node::File) {
            add(none, tr("&Attach Database…"), [this, s] { attach(s); });
            add(none, tr("&Load Extension…"), [this, s] { loadExtension(s); });
            menu.addSeparator();
            menu.addAction(m_refresh);
            menu.addAction(m_close);
        } else {
            if (schema != QLatin1String("main") && schema != QLatin1String("temp"))
                add(none, tr("&Detach Database"), [this, s, schema] { detach(s, schema); });
            menu.addAction(m_refresh);
        }
        break;
    case Node::Folder:
        switch (t.folder) {
        case Folder::Tables:
            add(Icons::object(ObjectKind::Table), tr("Create &Table…"),
                [this, s, schema] { createTable(s, schema); });
            add(Icons::importData(), tr("&Import CSV into a New Table…"),
                [this, s, schema] { importCsv(s, schema, QString()); });
            break;
        case Folder::Views:
            add(Icons::object(ObjectKind::View), tr("Create &View…"),
                [this, s, schema] { createView(s, schema); });
            break;
        case Folder::Indexes:
            add(Icons::object(ObjectKind::Index), tr("Create &Index…"),
                [this, s, schema, table = t.table] { createIndex(s, schema, table); });
            break;
        case Folder::Triggers: {
            const catalog::SnapshotPtr snapshot = s->snapshot();
            const catalog::SchemaObject *owner
                = snapshot ? snapshot->find(schema, t.table) : nullptr;
            const bool onView = owner && owner->kind == ObjectKind::View;
            add(Icons::object(ObjectKind::Trigger), tr("Create T&rigger…"),
                [this, s, schema, table = t.table, onView] {
                    createTrigger(s, schema, table, onView);
                });
            break;
        }
        case Folder::System:
        case Folder::Columns:
            break;
        }
        menu.addAction(m_refresh);
        break;
    case Node::Object: {
        const QString name = t.name;
        const ObjectKind kind = t.kind;
        const bool table = kind == ObjectKind::Table || kind == ObjectKind::VirtualTable;
        add(table || kind == ObjectKind::View
                ? Icons::object(kind)
                : QIcon::fromTheme(QStringLiteral("dialog-information")),
            tr("&Open"), [this, s, schema, name] { Q_EMIT objectRequested(s, schema, name); });
        if (kind == ObjectKind::Table) {
            add(Icons::diagram(), tr("&Diagram"),
                [this, s, schema, name] { Q_EMIT diagramRequested(s, schema, name); });
        }
        if (table || kind == ObjectKind::View) {
            add(QIcon::fromTheme(QStringLiteral("document-new")), tr("New SQL &Editor with SELECT"),
                [this, s, schema, name] {
                    const QString qualified = schema == QLatin1String("main")
                        ? sql::identifier(name)
                        : sql::identifier(schema) + QLatin1Char('.') + sql::identifier(name);
                    Q_EMIT editorRequested(s, QStringLiteral("SELECT * FROM %1;").arg(qualified));
                });
        }
        menu.addSeparator();
        if (t.system) {
            copyName();
            break;
        }
        switch (kind) {
        case ObjectKind::Table:
            add(Icons::edit(), tr("&Alter Table…"),
                [this, s, schema, name] { alterTable(s, schema, name); });
            add(none, tr("Re&name Table…"),
                [this, s, schema, name] { renameTable(s, schema, name); });
            add(Icons::object(ObjectKind::Index), tr("Create &Index…"),
                [this, s, schema, name] { createIndex(s, schema, name); });
            add(Icons::object(ObjectKind::Trigger), tr("Create T&rigger…"),
                [this, s, schema, name] { createTrigger(s, schema, name, false); });
            menu.addSeparator();
            add(Icons::importData(), tr("Im&port CSV…"),
                [this, s, schema, name] { importCsv(s, schema, name); });
            add(Icons::add(), tr("&Populate…"),
                [this, s, schema, name] { populate(s, schema, name); });
            add(none, tr("Rein&dex"), [this, s, schema, name] { reindex(s, schema, name); });
            menu.addSeparator();
            add(QIcon::fromTheme(QStringLiteral("edit-clear-all")), tr("E&mpty Table…"),
                [this, s, schema, name] { emptyTable(s, schema, name); });
            break;
        case ObjectKind::VirtualTable:
            break;
        case ObjectKind::View:
            add(Icons::edit(), tr("&Alter View…"),
                [this, s, schema, name] { alterView(s, schema, name); });
            add(Icons::object(ObjectKind::Trigger), tr("Create T&rigger…"),
                [this, s, schema, name] { createTrigger(s, schema, name, true); });
            break;
        case ObjectKind::Index:
            add(none, tr("Rein&dex"), [this, s, schema, name] { reindex(s, schema, name); });
            break;
        case ObjectKind::Trigger:
            add(Icons::edit(), tr("&Alter Trigger…"),
                [this, s, schema, name] { alterTrigger(s, schema, name); });
            break;
        default:
            break;
        }
        copyName();
        menu.addSeparator();
        add(QIcon::fromTheme(QStringLiteral("edit-delete")),
            tr("D&rop %1…").arg(catalog::kindName(kind)),
            [this, s, kind, schema, name] { drop(s, kind, schema, name); });
        break;
    }
    case Node::Column:
        copyName();
        break;
    }
}

bool DatabaseBrowser::confirm(const QString &title, const QString &question)
{
    if (m_confirm)
        return m_confirm(question);
    return QMessageBox::question(this, title, question, QMessageBox::Yes | QMessageBox::No,
                                 QMessageBox::No)
        == QMessageBox::Yes;
}

void DatabaseBrowser::runSql(Session *session, const QString &sql, const QString &done)
{
    if (!session || !session->runner())
        return;
    session->runner()->run(sql.toUtf8(), this, [this, sql, done](const sqlite::QueryOutcome &o) {
        if (o.ok()) {
            Q_EMIT message(done, false);
        } else {
            Q_EMIT message(o.error, true);
            QMessageBox::warning(this, tr("SQL Error"),
                                 QStringLiteral("%1\n\n%2").arg(o.error, sql));
        }
    });
}

// The menus' actions.

void DatabaseBrowser::openWithDialog()
{
    QSettings &settings = Settings::instance().store();
    const QStringList paths = QFileDialog::getOpenFileNames(
        this, tr("Open Database"), settings.value(DirectoryKey).toString(), databaseFilter());
    for (const QString &path : paths)
        openDatabase(path);
    if (!paths.isEmpty())
        settings.setValue(DirectoryKey, QFileInfo(paths.first()).absolutePath());
}

void DatabaseBrowser::newDatabase()
{
    QSettings &settings = Settings::instance().store();
    const QString path = QFileDialog::getSaveFileName(
        this, tr("New Database"), settings.value(DirectoryKey).toString(), databaseFilter());
    if (path.isEmpty())
        return;
    settings.setValue(DirectoryKey, QFileInfo(path).absolutePath());
    // The dialog asked already whether to replace it.
    if (QFileInfo::exists(path) && !QFile::remove(path)) {
        QMessageBox::warning(this, tr("New Database"), tr("Could not replace %1.").arg(path));
        return;
    }
    openDatabase(path, true);
}

void DatabaseBrowser::createTable(Session *session, const QString &schema)
{
    if (!session || !session->isOpen())
        return;
    TableDialog dialog(session, schema, this);
    dialog.exec();
}

void DatabaseBrowser::alterTable(Session *session, const QString &schema, const QString &table)
{
    if (!session || !session->isOpen())
        return;
    TableDialog dialog(session, schema, table, this);
    if (dialog.isValid())
        dialog.exec();
}

void DatabaseBrowser::renameTable(Session *session, const QString &schema, const QString &table)
{
    bool ok = false;
    const QString name
        = QInputDialog::getText(this, tr("Rename Table"), tr("New name of %1:").arg(table),
                                QLineEdit::Normal, table, &ok)
              .trimmed();
    if (!ok || name.isEmpty() || name == table)
        return;
    runSql(session,
           QStringLiteral("ALTER TABLE %1.%2 RENAME TO %3")
               .arg(sql::identifier(schema), sql::identifier(table), sql::identifier(name)),
           tr("Renamed %1 to %2.").arg(table, name));
}

void DatabaseBrowser::createView(Session *session, const QString &schema, const QString &select)
{
    if (!session || !session->isOpen())
        return;
    SqlObjectDialog dialog(session, SqlObjectDialog::Kind::View, schema, QString(), QString(),
                           this);
    if (!select.isEmpty())
        dialog.setViewQuery(select);
    dialog.exec();
}

void DatabaseBrowser::alterView(Session *session, const QString &schema, const QString &view)
{
    if (!session || !session->isOpen())
        return;
    SqlObjectDialog dialog(session, SqlObjectDialog::Kind::View, schema, view, QString(), this);
    dialog.exec();
}

void DatabaseBrowser::createIndex(Session *session, const QString &schema, const QString &table)
{
    if (!session || !session->isOpen())
        return;
    IndexDialog dialog(session, schema, table, this);
    dialog.exec();
}

void DatabaseBrowser::createTrigger(Session *session, const QString &schema, const QString &table,
                                    bool onView)
{
    if (!session || !session->isOpen())
        return;
    SqlObjectDialog dialog(session, SqlObjectDialog::Kind::Trigger, schema, QString(), table, this);
    dialog.setTriggerTemplate(onView);
    dialog.exec();
}

void DatabaseBrowser::alterTrigger(Session *session, const QString &schema, const QString &trigger)
{
    if (!session || !session->isOpen())
        return;
    SqlObjectDialog dialog(session, SqlObjectDialog::Kind::Trigger, schema, trigger, QString(),
                           this);
    dialog.exec();
}

void DatabaseBrowser::drop(Session *session, ObjectKind kind, const QString &schema,
                           const QString &name)
{
    if (!confirm(tr("Drop"),
                 tr("Drop the %1 %2? This cannot be undone unless a transaction is open.")
                     .arg(catalog::kindName(kind), name)))
        return;
    runSql(session, catalog::dropStatement(kind, schema, name),
           tr("Dropped the %1 %2.").arg(catalog::kindName(kind), name));
}

void DatabaseBrowser::emptyTable(Session *session, const QString &schema, const QString &table)
{
    if (!confirm(tr("Empty Table"), tr("Delete every row of %1?").arg(table)))
        return;
    runSql(session,
           QStringLiteral("DELETE FROM %1.%2").arg(sql::identifier(schema), sql::identifier(table)),
           tr("Emptied %1.").arg(table));
}

void DatabaseBrowser::reindex(Session *session, const QString &schema, const QString &name)
{
    runSql(session,
           QStringLiteral("REINDEX %1.%2").arg(sql::identifier(schema), sql::identifier(name)),
           tr("Reindexed %1.").arg(name));
}

void DatabaseBrowser::populate(Session *session, const QString &schema, const QString &table)
{
    if (!session || !session->isOpen())
        return;
    PopulatorDialog dialog(session, schema, table, this);
    if (dialog.isValid())
        dialog.exec();
}

void DatabaseBrowser::importCsv(Session *session, const QString &schema, const QString &table)
{
    if (!session || !session->isOpen())
        return;
    auto *dialog = new ImportDialog(session, schema, table, this);
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->show();
}

void DatabaseBrowser::attach(Session *session)
{
    if (!session || !session->isOpen())
        return;
    QSettings &settings = Settings::instance().store();
    const QString path = QFileDialog::getOpenFileName(
        this, tr("Attach Database"), settings.value(DirectoryKey).toString(), databaseFilter());
    if (path.isEmpty())
        return;
    bool ok = false;
    const QString name
        = QInputDialog::getText(this, tr("Attach Database"), tr("The name it goes by in SQL:"),
                                QLineEdit::Normal, QFileInfo(path).completeBaseName(), &ok)
              .trimmed();
    if (!ok || name.isEmpty())
        return;
    runSql(session,
           QStringLiteral("ATTACH DATABASE %1 AS %2")
               .arg(sqlite::quoteLiteral(path), sql::identifier(name)),
           tr("Attached %1 as %2.").arg(path, name));
}

void DatabaseBrowser::detach(Session *session, const QString &schema)
{
    runSql(session, QStringLiteral("DETACH DATABASE %1").arg(sql::identifier(schema)),
           tr("Detached %1.").arg(schema));
}

void DatabaseBrowser::loadExtension(Session *session)
{
    if (!session || !session->isOpen())
        return;
    if (!Settings::instance().preferences().allowExtensions) {
        QMessageBox::information(this, tr("Load Extension"),
                                 tr("Loading extensions is off. Turn it on in the preferences, "
                                    "then open the database again."));
        return;
    }
#if defined(Q_OS_WIN)
    const QString filter = tr("SQLite extensions (*.dll);;All files (*)");
#elif defined(Q_OS_MACOS)
    const QString filter = tr("SQLite extensions (*.dylib *.so);;All files (*)");
#else
    const QString filter = tr("SQLite extensions (*.so);;All files (*)");
#endif
    const QStringList paths
        = QFileDialog::getOpenFileNames(this, tr("Load Extension"), QString(), filter);
    for (const QString &path : paths) {
        session->loadExtension(path, this, [this, path](const QString &error) {
            if (error.isEmpty()) {
                Q_EMIT message(tr("Loaded the extension %1.").arg(path), false);
            } else {
                Q_EMIT message(error, true);
                QMessageBox::warning(this, tr("Load Extension"),
                                     tr("Could not load %1:\n%2").arg(path, error));
            }
        });
    }
}

void DatabaseBrowser::dump(Session *session, const QString &schema, bool schemaOnly)
{
    if (!session || !session->runner())
        return;
    QSettings &settings = Settings::instance().store();
    const QString suggested = settings.value(QStringLiteral("files/lastDirectory")).toString()
        + QLatin1Char('/') + QFileInfo(session->path()).completeBaseName()
        + (schemaOnly ? QStringLiteral("-schema.sql") : QStringLiteral(".sql"));
    const QString path
        = QFileDialog::getSaveFileName(this, schemaOnly ? tr("Export Schema") : tr("Dump Database"),
                                       suggested, tr("SQL scripts (*.sql);;All files (*)"));
    if (path.isEmpty())
        return;
    settings.setValue(QStringLiteral("files/lastDirectory"), QFileInfo(path).absolutePath());
    catalog::DumpOptions options;
    options.schema = schema;
    options.data = !schemaOnly;
    Q_EMIT message(tr("Writing %1…").arg(path), false);
    session->runner()->perform(
        catalog::dumpTask(options, path), this, [this, path](const sqlite::QueryOutcome &o) {
            if (o.ok()) {
                Q_EMIT message(tr("Wrote %1.").arg(path), false);
            } else {
                Q_EMIT message(o.error, true);
                QMessageBox::warning(this, tr("Dump"),
                                     tr("Could not write %1:\n%2").arg(path, o.error));
            }
        });
}

void DatabaseBrowser::maintenance(Session *session, const QString &schema)
{
    if (!session || !session->isOpen())
        return;
    MaintenanceDialog dialog(session, schema, this);
    dialog.exec();
}

} // namespace sqliteman
