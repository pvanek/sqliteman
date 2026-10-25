// SPDX-FileCopyrightText: 2007-2026 Petr Vanek
// SPDX-License-Identifier: GPL-3.0-or-later

#include "SqlObjectDialog.h"

#include "Session.h"
#include "SqlEditor.h"
#include "catalog/Details.h"
#include "sql/Keywords.h"
#include "sql/Lexer.h"
#include "sqlite/Direct.h"

#include "ui_SqlObjectDialog.h"

#include <QPushButton>

#include <sqlite3.h>

namespace sqliteman {

namespace {

// The SELECT of a CREATE VIEW: what follows its AS.
QString viewQuery(const QString &createView)
{
    const QByteArray utf8 = createView.toUtf8();
    for (const sql::Token &t : sql::tokenize(utf8)) {
        if (t.kind == sql::TokenKind::Keyword
            && QByteArrayView(utf8).sliced(t.offset, t.length).compare("AS", Qt::CaseInsensitive)
                == 0)
            return QString::fromUtf8(utf8.mid(t.end())).trimmed();
    }
    return createView;
}

} // namespace

SqlObjectDialog::SqlObjectDialog(Session *session, Kind kind, const QString &schema,
                                 const QString &name, const QString &table, QWidget *parent)
    : QDialog(parent), m_ui(std::make_unique<Ui::SqlObjectDialog>()), m_session(session),
      m_kind(kind), m_schema(schema.isEmpty() ? QStringLiteral("main") : schema), m_original(name),
      m_table(table)
{
    m_ui->setupUi(this);
    const bool view = kind == Kind::View;
    if (view) {
        setWindowTitle(isAltering() ? tr("Alter View %1").arg(name) : tr("Create View"));
        m_ui->hint->setText(tr("The query the view shows the rows of."));
    } else {
        setWindowTitle(isAltering() ? tr("Alter Trigger %1").arg(name) : tr("Create Trigger"));
        m_ui->hint->setText(tr("The whole CREATE TRIGGER statement. NEW and OLD are the row as "
                               "it is after and before the change."));
        m_ui->nameLabel->hide();
        m_ui->name->hide();
    }
    m_ui->buttons->button(QDialogButtonBox::Ok)
        ->setText(isAltering() ? tr("Alte&r") : tr("C&reate"));

    if (isAltering()) {
        const catalog::SnapshotPtr snapshot = session ? session->snapshot() : nullptr;
        const catalog::DatabaseInfo *database = snapshot ? snapshot->database(m_schema) : nullptr;
        if (database) {
            for (const catalog::SchemaObject &o : database->objects) {
                if (o.name.compare(name, Qt::CaseInsensitive) != 0)
                    continue;
                m_ui->sql->setText(view ? viewQuery(o.sql) : o.sql);
                break;
            }
        }
        m_ui->name->setText(name);
    }
    connect(m_ui->name, &QLineEdit::textChanged, this, &SqlObjectDialog::updateEnabled);
    connect(m_ui->sql, &QsciScintilla::textChanged, this, &SqlObjectDialog::updateEnabled);
    connect(m_ui->buttons, &QDialogButtonBox::accepted, this, [this] {
        apply([this](bool ok) {
            if (ok)
                accept();
        });
    });
    connect(m_ui->buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    updateEnabled();
}

SqlObjectDialog::~SqlObjectDialog() = default;

void SqlObjectDialog::setViewQuery(const QString &select)
{
    m_ui->sql->setText(select.trimmed());
}

void SqlObjectDialog::setTriggerTemplate(bool onView)
{
    m_ui->sql->setText(catalog::triggerTemplate(m_schema, m_table, onView));
}

void SqlObjectDialog::setName(const QString &name)
{
    m_ui->name->setText(name);
}

SqlEditor *SqlObjectDialog::editor() const
{
    return m_ui->sql;
}

QString SqlObjectDialog::statement() const
{
    if (m_kind == Kind::Trigger)
        return m_ui->sql->text().trimmed();
    if (m_ui->name->text().trimmed().isEmpty() || m_ui->sql->text().trimmed().isEmpty())
        return {};
    return catalog::createViewStatement(m_schema, m_ui->name->text().trimmed(), m_ui->sql->text());
}

QString SqlObjectDialog::statusText() const
{
    return m_ui->status->text();
}

void SqlObjectDialog::updateEnabled()
{
    m_ui->buttons->button(QDialogButtonBox::Ok)->setEnabled(!statement().isEmpty());
}

void SqlObjectDialog::apply(std::function<void(bool ok)> done)
{
    const QString create = statement();
    if (create.isEmpty() || !m_session || !m_session->runner()) {
        if (done)
            done(false);
        return;
    }
    const QString drop = isAltering()
        ? catalog::dropStatement(m_kind == Kind::View ? catalog::ObjectKind::View
                                                      : catalog::ObjectKind::Trigger,
                                 m_schema, m_original)
        : QString();
    m_session->runner()->perform(
        [drop, create](sqlite3 *db) {
            sqlite::TaskOutcome outcome;
            // Both or neither: a savepoint nests in a transaction already open.
            outcome.error = sqlite::exec(db, "SAVEPOINT sqliteman_replace");
            if (outcome.error.isEmpty() && !drop.isEmpty())
                outcome.error = sqlite::exec(db, drop.toUtf8());
            if (outcome.error.isEmpty())
                outcome.error = sqlite::exec(db, create.toUtf8());
            if (outcome.error.isEmpty()) {
                outcome.error = sqlite::exec(db, "RELEASE sqliteman_replace");
            } else {
                sqlite::exec(db, "ROLLBACK TO sqliteman_replace");
                sqlite::exec(db, "RELEASE sqliteman_replace");
            }
            return outcome;
        },
        this,
        [this, done](const sqlite::QueryOutcome &o) {
            const bool view = m_kind == Kind::View;
            if (o.ok()) {
                m_ui->status->setText(view ? tr("View saved.") : tr("Trigger saved."));
                // Not seen by the connection's own watch: a task, not SQL.
                if (m_session)
                    m_session->reloadSnapshot();
            } else {
                m_ui->status->setText(o.error);
            }
            if (done)
                done(o.ok());
        });
}

} // namespace sqliteman
