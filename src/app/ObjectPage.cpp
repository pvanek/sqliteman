// SPDX-FileCopyrightText: 2026 Petr Vanek
// SPDX-License-Identifier: GPL-3.0-or-later

#include "ObjectPage.h"

#include "DiagramFile.h"
#include "ErdView.h"
#include "Icons.h"
#include "Session.h"
#include "Shortcuts.h"
#include "SqlEditor.h"
#include "TableData.h"
#include "catalog/Erd.h"
#include "catalog/ErdExport.h"
#include "sql/Keywords.h"

#include <QAction>
#include <QClipboard>
#include <QCoreApplication>
#include <QGuiApplication>
#include <QHeaderView>
#include <QLabel>
#include <QMenu>
#include <QTabWidget>
#include <QTableWidget>
#include <QToolBar>
#include <QToolButton>
#include <QVBoxLayout>

#include <sqlite3.h>

namespace sqliteman {

using catalog::ObjectKind;

namespace {

QTableWidget *table(QWidget *parent, const QStringList &headers)
{
    auto *t = new QTableWidget(parent);
    t->setColumnCount(int(headers.size()));
    t->setHorizontalHeaderLabels(headers);
    t->verticalHeader()->hide();
    t->horizontalHeader()->setStretchLastSection(true);
    t->setEditTriggers(QAbstractItemView::NoEditTriggers);
    t->setSelectionBehavior(QAbstractItemView::SelectRows);
    t->setAlternatingRowColors(true);
    return t;
}

void addRow(QTableWidget *t, const QStringList &cells)
{
    const int row = t->rowCount();
    t->insertRow(row);
    for (int c = 0; c < cells.size(); ++c)
        t->setItem(row, c, new QTableWidgetItem(cells[c]));
}

QString yes(bool on)
{
    return on ? QCoreApplication::translate("ObjectPage", "yes") : QString();
}

bool hasRows(ObjectKind kind)
{
    return kind == ObjectKind::Table || kind == ObjectKind::VirtualTable
        || kind == ObjectKind::View;
}

} // namespace

ObjectPage::ObjectPage(Session *session, const QString &schema, const QString &name,
                       QWidget *parent)
    : WorkspacePage(parent), m_session(session), m_schema(schema), m_name(name),
      m_summary(new QLabel(this)), m_tabs(new QTabWidget(this)), m_sql(new SqlEditor(this))
{
    if (name.isEmpty()) {
        m_kind = ObjectKind::Database;
    } else if (m_session && m_session->snapshot()) {
        if (const catalog::SchemaObject *o = m_session->snapshot()->find(schema, name))
            m_kind = o->kind;
    }
    build();
    if (m_session) {
        setToolTip(m_session->path());
        connect(m_session, &Session::stateChanged, this, &ObjectPage::titleChanged);
        connect(m_session, &Session::snapshotChanged, this, [this] {
            // What a database holds is the snapshot's; so is a diagram.
            if (isDatabase())
                refresh();
            if (m_diagramLoaded)
                loadDiagram();
        });
    }
    refresh();
}

void ObjectPage::build()
{
    m_sql->setReadOnly(true);
    m_tabs->setDocumentMode(true);
    if (isDatabase()) {
        m_properties = table(this, {tr("Property"), tr("Value")});
        m_objects = table(this, {tr("Name"), tr("Kind"), tr("Of"), tr("Columns"), tr("Notes")});
        m_objects->setToolTip(tr("Double-click an object to open it"));
        connect(m_objects, &QTableWidget::cellActivated, this, [this](int row, int) {
            if (QTableWidgetItem *item = m_objects->item(row, 0))
                Q_EMIT objectRequested(m_session, m_schema, item->text());
        });
        m_tabs->addTab(m_properties, tr("&Properties"));
        m_tabs->addTab(m_objects, tr("&Objects"));
        m_tabs->addTab(m_sql, tr("&Schema"));
    } else {
        if (hasRows(m_kind)) {
            m_data = new TableData(m_session, m_schema, m_name, m_kind, this);
            m_tabs->addTab(m_data, Icons::object(m_kind), tr("&Data"));
        }
        m_columns = table(this,
                          {tr("Name"), tr("Type"), tr("Not null"), tr("Default"), tr("Primary key"),
                           tr("Generated")});
        m_indexes
            = table(this, {tr("Name"), tr("Unique"), tr("Made by"), tr("Partial"), tr("Columns")});
        m_foreignKeys
            = table(this, {tr("Columns"), tr("References"), tr("On update"), tr("On delete")});
        m_triggers = table(this, {tr("Name"), tr("SQL")});
        m_tabs->addTab(m_sql, tr("&SQL"));
        m_tabs->addTab(m_columns, tr("&Columns"));
        m_tabs->addTab(m_indexes, tr("&Indexes"));
        m_tabs->addTab(m_foreignKeys, tr("&Foreign Keys"));
        m_tabs->addTab(m_triggers, tr("&Triggers"));
    }

    if (catalog::hasDiagram(m_kind)) {
        m_diagramTab = new QWidget(this);
        m_diagram = new ErdView(m_diagramTab);
        auto *bar = new QToolBar(m_diagramTab);
        bar->setIconSize(QSize(16, 16));
        auto zoom = [&](const QString &icon, const QString &text, auto slot) {
            QAction *action = bar->addAction(QIcon::fromTheme(icon), text);
            connect(action, &QAction::triggered, m_diagram, slot);
        };
        zoom(QStringLiteral("zoom-fit-best"), tr("Fit"), &ErdView::fitDiagram);
        zoom(QStringLiteral("zoom-in"), tr("Zoom In"), &ErdView::zoomIn);
        zoom(QStringLiteral("zoom-out"), tr("Zoom Out"), &ErdView::zoomOut);
        zoom(QStringLiteral("zoom-original"), tr("Actual Size"), &ErdView::resetZoom);
        bar->addSeparator();
        // Text formats are mostly pasted into documentation, so they can go
        // straight to the clipboard; pictures only make sense as files.
        auto *exportMenu = new QMenu(bar);
        exportMenu->setObjectName(QStringLiteral("diagramExportMenu"));
        auto copy = [&](const QString &objectName, const QString &text, auto toText) {
            QAction *action = exportMenu->addAction(text);
            action->setObjectName(objectName);
            connect(action, &QAction::triggered, this, [this, toText] {
                if (!m_diagram->graph().isEmpty())
                    QGuiApplication::clipboard()->setText(toText(m_diagram->graph()));
            });
        };
        copy(QStringLiteral("copyMermaid"), tr("Copy as Mermaid"), &catalog::erdToMermaid);
        copy(QStringLiteral("copyGraphviz"), tr("Copy as Graphviz"), &catalog::erdToDot);
        exportMenu->addSeparator();
        const QStringList filters = ErdView::fileFilters();
        const QStringList saveTexts = {tr("Save as SVG…"), tr("Save as PNG…"), tr("Save as PDF…"),
                                       tr("Save as Graphviz…"), tr("Save as Mermaid…")};
        for (int i = 0; i < filters.size(); ++i) {
            QAction *action = exportMenu->addAction(saveTexts.value(i, filters[i]));
            connect(action, &QAction::triggered, this,
                    [this, filter = filters[i]] { exportDiagram(filter); });
        }
        connect(exportMenu, &QMenu::aboutToShow, this, [this, exportMenu] {
            for (QAction *action : exportMenu->actions())
                action->setEnabled(!m_diagram->graph().isEmpty());
        });
        auto *exportButton = new QToolButton(bar);
        exportButton->setIcon(
            QIcon::fromTheme(QStringLiteral("document-export"),
                             QIcon::fromTheme(QStringLiteral("document-save-as"))));
        exportButton->setText(tr("Export"));
        exportButton->setToolTip(
            tr("Copy the diagram as Mermaid or Graphviz, or save it to a file"));
        exportButton->setMenu(exportMenu);
        exportButton->setPopupMode(QToolButton::InstantPopup);
        bar->addWidget(exportButton);
        auto *hint = new QLabel(tr("Drag to move a table, Ctrl+wheel to zoom, "
                                   "double-click a table to open it."),
                                m_diagramTab);
        hint->setEnabled(false);
        bar->addWidget(hint);
        auto *diagramLayout = new QVBoxLayout(m_diagramTab);
        diagramLayout->setContentsMargins(0, 0, 0, 0);
        diagramLayout->setSpacing(0);
        diagramLayout->addWidget(bar);
        diagramLayout->addWidget(m_diagram, 1);
        m_tabs->addTab(m_diagramTab, tr("D&iagram"));
        connect(m_diagram, &ErdView::tableActivated, this, [this](unsigned int id) {
            if (const catalog::ErdNode *node = m_diagram->graph().node(id))
                Q_EMIT objectRequested(m_session, node->schema, node->name);
        });
    }
    connect(m_tabs, &QTabWidget::currentChanged, this, [this] {
        if (m_diagramTab && m_tabs->currentWidget() == m_diagramTab && !m_diagramLoaded)
            loadDiagram();
        updateActions();
    });

    m_summary->setTextFormat(Qt::PlainText);
    m_summary->setWordWrap(true);
    m_summary->setMargin(4);
    m_summary->setTextInteractionFlags(Qt::TextSelectableByMouse);
    QFont bold = m_summary->font();
    bold.setBold(true);
    m_summary->setFont(bold);

    auto *toolbar = new QToolBar(this);
    toolbar->setIconSize(QSize(16, 16));
    m_refresh = toolbar->addAction(Icons::refresh(), tr("Refresh"), this, &ObjectPage::refresh);
    m_refresh->setToolTip(tr("Read what is shown again"));
    // The rows have a refresh of their own, with the same keys: while their
    // tab is shown, this one is off, or neither would know it was meant.
    m_refresh->setShortcuts(Shortcuts::refresh());
    m_refresh->setShortcutContext(Qt::WidgetWithChildrenShortcut);
    addAction(m_refresh);
    if (hasRows(m_kind)) {
        m_count = toolbar->addAction(QIcon::fromTheme(QStringLiteral("format-list-ordered")),
                                     tr("Count Rows"), this, &ObjectPage::countRows);
        m_count->setToolTip(tr("Count the rows: a full scan, slow on a big table"));
    }

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    layout->addWidget(toolbar);
    layout->addWidget(m_summary);
    layout->addWidget(m_tabs);
    updateActions();
}

QString ObjectPage::title() const
{
    const QString session = m_session ? m_session->name() : QString();
    if (isDatabase() && !session.isEmpty())
        return m_schema == QLatin1String("main") ? session
                                                 : m_schema + QStringLiteral(" · ") + session;
    if (isDatabase())
        return m_schema;
    const QString name
        = m_schema == QLatin1String("main") ? m_name : m_schema + QLatin1Char('.') + m_name;
    return session.isEmpty() ? name : name + QStringLiteral(" · ") + session;
}

QIcon ObjectPage::icon() const
{
    if (isDatabase() || !m_details.object.name.isEmpty())
        return Icons::object(isDatabase() ? ObjectKind::Database : m_details.object.kind);
    return hasRows(m_kind) ? Icons::object(m_kind)
                           : QIcon::fromTheme(QStringLiteral("dialog-information"));
}

bool ObjectPage::maybeClose()
{
    if (m_data && !m_data->maybeClose()) {
        showData();
        return false;
    }
    return true;
}

void ObjectPage::showData()
{
    if (m_data)
        m_tabs->setCurrentWidget(m_data);
}

void ObjectPage::showDiagram()
{
    if (m_diagramTab)
        m_tabs->setCurrentWidget(m_diagramTab);
}

void ObjectPage::updateActions()
{
    m_refresh->setEnabled(!m_data || m_tabs->currentWidget() != m_data);
}

void ObjectPage::refresh()
{
    if (!m_session || !m_session->runner()) {
        m_summary->setText(tr("The database is closed."));
        return;
    }
    if (isDatabase()) {
        m_session->runner()->perform(
            [schema = m_schema](sqlite3 *db) {
                sqlite::TaskOutcome outcome;
                outcome.value = QVariant::fromValue(catalog::loadDatabaseDetails(db, schema));
                return outcome;
            },
            this,
            [this](const sqlite::QueryOutcome &o) {
                if (!o.ok()) {
                    m_summary->setText(o.error);
                    return;
                }
                showDatabase(o.task.value.value<catalog::DatabaseDetails>());
            });
        return;
    }
    m_session->runner()->perform(
        [schema = m_schema, name = m_name](sqlite3 *db) {
            sqlite::TaskOutcome outcome;
            outcome.value = QVariant::fromValue(catalog::loadDetails(db, schema, name));
            return outcome;
        },
        this,
        [this](const sqlite::QueryOutcome &o) {
            if (!o.ok()) {
                m_summary->setText(o.error);
                return;
            }
            showObject(o.task.value.value<catalog::ObjectDetails>());
        });
}

void ObjectPage::showDatabase(const catalog::DatabaseDetails &details)
{
    m_database = details;
    if (!details.error.isEmpty()) {
        m_summary->setText(details.error);
        return;
    }
    const catalog::DatabaseInfo &d = details.database;
    m_summary->setText(QStringLiteral("%1 %2").arg(catalog::kindName(ObjectKind::Database), d.name)
                       + (d.file.isEmpty() ? QString() : QStringLiteral(", ") + d.file));

    m_properties->setRowCount(0);
    for (const auto &[name, value] : details.properties)
        addRow(m_properties, {name, value});

    m_objects->setRowCount(0);
    QString schema;
    for (const catalog::SchemaObject &o : d.objects) {
        QStringList notes;
        if (o.system)
            notes << tr("SQLite's own");
        if (o.withoutRowid)
            notes << tr("without rowid");
        if (o.strict)
            notes << tr("strict");
        if (!o.foreignKeys.empty())
            notes << tr("%n foreign key(s)", nullptr, int(o.foreignKeys.size()));
        if (!o.error.isEmpty())
            notes << o.error;
        const bool belongs = o.kind == ObjectKind::Index || o.kind == ObjectKind::Trigger;
        addRow(m_objects,
               {o.name, catalog::kindName(o.kind), belongs ? o.table : QString(),
                hasRows(o.kind) ? QString::number(o.columns.size()) : QString(),
                notes.join(QLatin1String(", "))});
        m_objects->item(m_objects->rowCount() - 1, 0)->setIcon(Icons::object(o.kind));
        if (!o.sql.isEmpty() && !o.system)
            schema += o.sql + QLatin1String(";\n\n");
    }
    m_sql->setReadOnly(false);
    m_sql->setText(schema.trimmed());
    m_sql->setReadOnly(true);
    for (QTableWidget *t : {m_properties, m_objects})
        t->resizeColumnsToContents();
    Q_EMIT titleChanged();
    Q_EMIT loaded();
}

void ObjectPage::showObject(const catalog::ObjectDetails &details)
{
    m_details = details;
    Q_EMIT titleChanged();
    if (!details.error.isEmpty()) {
        m_summary->setText(details.error);
        return;
    }
    const catalog::SchemaObject &o = details.object;
    QStringList facts {QStringLiteral("%1 %2.%3").arg(catalog::kindName(o.kind), o.schema, o.name)};
    if (o.kind == ObjectKind::Index || o.kind == ObjectKind::Trigger)
        facts << tr("on %1").arg(o.table);
    if (o.withoutRowid)
        facts << tr("without rowid");
    if (o.strict)
        facts << tr("strict");
    if (o.autoincrement)
        facts << tr("autoincrement");
    m_summary->setText(facts.join(QLatin1String(", ")));

    m_sql->setReadOnly(false);
    QString sql = o.sql.isEmpty() ? tr("-- Made by SQLite itself.") : o.sql + QLatin1Char(';');
    for (const catalog::SchemaObject &t : details.triggers)
        sql += QLatin1String("\n\n") + t.sql + QLatin1Char(';');
    for (const catalog::IndexDetails &i : details.indexes) {
        if (!i.sql.isEmpty() && o.kind != ObjectKind::Index)
            sql += QLatin1String("\n\n") + i.sql + QLatin1Char(';');
    }
    m_sql->setText(sql);
    m_sql->setReadOnly(true);

    m_columns->setRowCount(0);
    for (const catalog::ColumnInfo &c : o.columns) {
        addRow(m_columns,
               {c.name, c.type, yes(c.notNull), c.hasDefault ? c.defaultValue : QString(),
                c.primaryKey > 0 ? QString::number(c.primaryKey) : QString(),
                c.hidden == 2       ? tr("virtual")
                    : c.hidden == 3 ? tr("stored")
                                    : QString()});
    }
    m_indexes->setRowCount(0);
    for (const catalog::IndexDetails &i : details.indexes) {
        using Origin = catalog::IndexDetails::Origin;
        const QString origin = i.origin == Origin::PrimaryKey ? tr("primary key")
            : i.origin == Origin::Unique                      ? tr("UNIQUE constraint")
                                                              : tr("CREATE INDEX");
        addRow(
            m_indexes,
            {i.name, yes(i.unique), origin, yes(i.partial), i.columns.join(QLatin1String(", "))});
    }
    m_foreignKeys->setRowCount(0);
    for (const catalog::ForeignKey &k : details.foreignKeys) {
        const QString to = k.to.isEmpty()
            ? k.table
            : QStringLiteral("%1 (%2)").arg(k.table, k.to.join(QLatin1String(", ")));
        addRow(m_foreignKeys, {k.from.join(QLatin1String(", ")), to, k.onUpdate, k.onDelete});
    }
    m_triggers->setRowCount(0);
    for (const catalog::SchemaObject &t : details.triggers)
        addRow(m_triggers, {t.name, t.sql.simplified()});

    const bool relation = hasRows(o.kind);
    m_tabs->setTabVisible(m_tabs->indexOf(m_columns), relation);
    m_tabs->setTabVisible(m_tabs->indexOf(m_indexes),
                          o.kind == ObjectKind::Table || o.kind == ObjectKind::Index);
    m_tabs->setTabVisible(m_tabs->indexOf(m_foreignKeys), o.kind == ObjectKind::Table);
    m_tabs->setTabVisible(m_tabs->indexOf(m_triggers),
                          o.kind == ObjectKind::Table || o.kind == ObjectKind::View);
    for (QTableWidget *t : {m_columns, m_indexes, m_foreignKeys, m_triggers})
        t->resizeColumnsToContents();
    Q_EMIT loaded();
}

void ObjectPage::countRows()
{
    if (!m_session || !m_session->runner() || !hasRows(m_kind))
        return;
    const QByteArray sql = "SELECT count(*) FROM " + sql::identifier(m_schema).toUtf8() + '.'
        + sql::identifier(m_name).toUtf8();
    const QString before = m_summary->text().section(QLatin1String(" · "), 0, 0);
    m_summary->setText(before + QStringLiteral(" · ") + tr("counting…"));
    m_session->runner()->run(sql, this, [this, before](const sqlite::QueryOutcome &outcome) {
        if (!outcome.ok()) {
            m_summary->setText(before + QStringLiteral(" · ") + outcome.error);
            return;
        }
        const qint64 rows = outcome.rows().valueAt(0, 0).toInteger();
        m_summary->setText(before + QStringLiteral(" · ")
                           + tr("%1 row(s)").arg(QLocale().toString(rows)));
    });
}

void ObjectPage::loadDiagram()
{
    if (!m_diagram || !m_session || !m_session->snapshot())
        return;
    m_diagramLoaded = true;
    // Built from the snapshot the session keeps anyway: nothing to query.
    m_diagram->setGraph(catalog::erdGraph(
        *m_session->snapshot(),
        isDatabase() ? catalog::ErdScope::Database : catalog::ErdScope::Table, m_schema, m_name));
}

void ObjectPage::exportDiagram(const QString &filter)
{
    if (!m_diagram || m_diagram->graph().isEmpty())
        return;
    // A name made from the object's, with the suffix of the format chosen.
    exportToFile(this, tr("Export Diagram"), isDatabase() ? m_schema : m_name, filter,
                 [this](const QString &path, QString *error) {
                     return m_diagram->saveDiagram(path, error);
                 });
}

} // namespace sqliteman
