// SPDX-FileCopyrightText: 2007-2026 Petr Vanek
// SPDX-License-Identifier: GPL-3.0-or-later

#include "TableData.h"

#include "Icons.h"
#include "ResultModel.h"
#include "ResultView.h"
#include "SaveChangesDialog.h"
#include "Session.h"
#include "catalog/Editing.h"
#include "sql/Keywords.h"

#include <QAction>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QVBoxLayout>

#include <sqlite3.h>

namespace sqliteman {

TableData::TableData(Session *session, const QString &schema, const QString &name,
                     catalog::ObjectKind kind, QWidget *parent)
    : QWidget(parent), m_session(session), m_schema(schema), m_name(name), m_kind(kind),
      m_view(new ResultView(this)), m_filter(new QLineEdit(this)), m_order(new QLineEdit(this))
{
    m_filter->setPlaceholderText(tr("WHERE … (Enter to apply)"));
    m_filter->setClearButtonEnabled(true);
    m_filter->setToolTip(tr("Only the rows this condition holds for, as after WHERE"));
    m_order->setPlaceholderText(tr("ORDER BY …"));
    m_order->setClearButtonEnabled(true);
    m_order->setToolTip(tr("The order of the rows, as after ORDER BY"));
    connect(m_filter, &QLineEdit::returnPressed, this, &TableData::refresh);
    connect(m_order, &QLineEdit::returnPressed, this, &TableData::refresh);

    auto *bar = new QHBoxLayout;
    bar->setContentsMargins(4, 2, 4, 2);
    bar->addWidget(new QLabel(tr("Filter:"), this));
    bar->addWidget(m_filter, 3);
    bar->addWidget(new QLabel(tr("Order:"), this));
    bar->addWidget(m_order, 1);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    layout->addLayout(bar);
    layout->addWidget(m_view);

    if (kind == catalog::ObjectKind::Table) {
        auto *populate = new QAction(Icons::add(), tr("Populate…"), this);
        populate->setToolTip(tr("Fill the table with made-up rows"));
        connect(populate, &QAction::triggered, this,
                [this] { Q_EMIT populateRequested(m_session, m_schema, m_name); });
        auto *import = new QAction(Icons::importData(), tr("Import…"), this);
        import->setToolTip(tr("Add rows from a CSV file"));
        connect(import, &QAction::triggered, this,
                [this] { Q_EMIT importRequested(m_session, m_schema, m_name); });
        m_empty
            = new QAction(QIcon::fromTheme(QStringLiteral("edit-clear-all")), tr("Empty…"), this);
        m_empty->setToolTip(tr("Delete every row of the table"));
        connect(m_empty, &QAction::triggered, this, [this] {
            if (!m_session || !m_session->runner())
                return;
            if (QMessageBox::question(this, tr("Empty Table"),
                                      tr("Delete every row of %1? This cannot be undone unless a "
                                         "transaction is open.")
                                          .arg(m_name))
                != QMessageBox::Yes)
                return;
            const QByteArray sql = "DELETE FROM " + sql::identifier(m_schema).toUtf8() + '.'
                + sql::identifier(m_name).toUtf8();
            m_session->runner()->run(sql, this, [this](const sqlite::QueryOutcome &o) {
                m_view->setNote(o.ok()
                                    ? tr("Deleted %n row(s).", nullptr, o.results.back().changes())
                                    : o.error,
                                !o.ok());
                m_view->model()->discardChanges();
                refresh();
            });
        });
        m_view->addHeaderAction(populate);
        m_view->addHeaderAction(import);
        m_view->addHeaderAction(m_empty);
    }

    connect(m_view, &ResultView::saveRequested, this, [this] { saveChanges(); });
    connect(m_view, &ResultView::rowsShown, this, &TableData::updateEditTarget);
    if (m_session) {
        // A schema read anew may make it editable: when the rows are found
        // by the rowid, it has to be in them.
        connect(m_session, &Session::snapshotChanged, this, [this] {
            if (query() != m_view->sql() && !m_view->model()->hasChanges() && !m_view->isRunning())
                refresh();
            else
                updateEditTarget();
        });
    }
    refresh();
}

bool TableData::needsRowid() const
{
    if (m_kind != catalog::ObjectKind::Table || !m_session || !m_session->snapshot())
        return false;
    const catalog::SchemaObject *table = m_session->snapshot()->find(m_schema, m_name);
    if (!table || !table->hasRowid())
        return false;
    // A table whose primary key is an INTEGER PRIMARY KEY has the rowid as
    // that column already; one with another primary key is found by it.
    return table->primaryKey().empty();
}

QByteArray TableData::query() const
{
    QString sql = QStringLiteral("SELECT ");
    if (needsRowid())
        sql += QLatin1String("rowid, ");
    sql += QLatin1String("* FROM ") + sql::identifier(m_schema) + QLatin1Char('.')
        + sql::identifier(m_name);
    if (!m_filter->text().trimmed().isEmpty())
        sql += QLatin1String(" WHERE ") + m_filter->text().trimmed();
    if (!m_order->text().trimmed().isEmpty())
        sql += QLatin1String(" ORDER BY ") + m_order->text().trimmed();
    return sql.toUtf8();
}

void TableData::refresh()
{
    if (m_view->model()->hasChanges()
        && QMessageBox::question(this, tr("Refresh"),
                                 tr("Reading the rows again discards the changes not saved yet."),
                                 QMessageBox::Discard | QMessageBox::Cancel)
            != QMessageBox::Discard)
        return;
    m_view->model()->discardChanges();
    const QString title
        = m_schema == QLatin1String("main") ? m_name : m_schema + QLatin1Char('.') + m_name;
    m_view->run(m_session ? m_session->runner() : nullptr, title, query());
}

void TableData::updateEditTarget()
{
    ResultModel *model = m_view->model();
    if (!model->hasColumns() || model->hasChanges())
        return;
    catalog::EditTarget target;
    if (m_kind != catalog::ObjectKind::Table) {
        target.reason
            = tr("%1 is a %2: its rows are read-only here.").arg(m_name, catalog::kindName(m_kind));
    } else if (!m_session || !m_session->snapshot()) {
        target.reason = tr("The schema is still being read.");
    } else {
        target = catalog::editTarget(model->rows(), *m_session->snapshot());
    }
    model->setEditTarget(target);
    if (m_empty)
        m_empty->setEnabled(target.editable());
}

void TableData::saveChanges(bool confirm)
{
    ResultModel *model = m_view->model();
    if (!model->hasChanges() || !model->isEditable() || !m_session || !m_session->runner())
        return;
    const catalog::EditTarget &target = model->editTarget();
    const std::vector<catalog::DmlStatement> statements
        = catalog::dmlStatements(target, model->changes());
    if (confirm) {
        QStringList previews;
        for (const catalog::DmlStatement &s : statements)
            previews << s.preview();
        SaveChangesDialog dialog(target.schema + QLatin1Char('.') + target.table, previews,
                                 m_session->inTransaction(), this);
        if (dialog.exec() != QDialog::Accepted)
            return;
    }
    m_session->runner()->perform(
        [statements](sqlite3 *db) { return catalog::applyEdits(db, statements); }, this,
        [this](const sqlite::QueryOutcome &outcome) {
            if (!outcome.ok()) {
                m_view->setNote(tr("Nothing was saved: %1").arg(outcome.error), true);
                return;
            }
            m_view->setNote(tr("Saved %n change(s).", nullptr, outcome.task.value.toInt()));
            m_view->model()->discardChanges();
            refresh();
        });
}

bool TableData::maybeClose()
{
    if (!m_view->model()->hasChanges())
        return true;
    const auto answer = QMessageBox::question(
        this, tr("Unsaved Changes"), tr("Save the changes to the rows of %1?").arg(m_name),
        QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel, QMessageBox::Save);
    if (answer == QMessageBox::Save) {
        saveChanges(false);
        return true;
    }
    return answer == QMessageBox::Discard;
}

} // namespace sqliteman
