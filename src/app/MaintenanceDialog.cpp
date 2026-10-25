// SPDX-FileCopyrightText: 2007-2026 Petr Vanek
// SPDX-License-Identifier: GPL-3.0-or-later

#include "MaintenanceDialog.h"

#include "Session.h"
#include "Settings.h"
#include "sql/Keywords.h"
#include "sqlite/Quote.h"

#include "ui_MaintenanceDialog.h"

#include <QFileDialog>
#include <QFileInfo>
#include <QPushButton>
#include <QTime>

namespace sqliteman {

MaintenanceDialog::MaintenanceDialog(Session *session, const QString &schema, QWidget *parent)
    : QDialog(parent), m_ui(std::make_unique<Ui::MaintenanceDialog>()), m_session(session),
      m_schema(schema.isEmpty() ? QStringLiteral("main") : schema)
{
    m_ui->setupUi(this);
    setWindowTitle(tr("Maintenance of %1").arg(m_schema));
    m_ui->log->setFont(Settings::instance().editorFont());
    if (const catalog::SnapshotPtr snapshot = session ? session->snapshot() : nullptr) {
        if (const catalog::DatabaseInfo *d = snapshot->database(m_schema)) {
            for (const catalog::SchemaObject &o : d->objects) {
                if (o.kind == catalog::ObjectKind::Table && !o.system)
                    m_ui->tables->addItem(o.name);
            }
        }
    }
    const QString s = sql::identifier(m_schema);
    connect(m_ui->analyze, &QPushButton::clicked, this,
            [this] { forEachTable(QStringLiteral("ANALYZE %1"), tr("Analyze")); });
    connect(m_ui->reindex, &QPushButton::clicked, this,
            [this] { forEachTable(QStringLiteral("REINDEX %1"), tr("Reindex")); });
    connect(m_ui->dropStatistics, &QPushButton::clicked, this, [this, s] {
        run(QStringLiteral("DELETE FROM %1.sqlite_stat1").arg(s), tr("Drop statistics"));
    });
    connect(m_ui->optimize, &QPushButton::clicked, this,
            [this, s] { run(QStringLiteral("PRAGMA %1.optimize").arg(s), tr("Optimize")); });
    connect(m_ui->vacuum, &QPushButton::clicked, this,
            [this, s] { run(QStringLiteral("VACUUM %1").arg(s), tr("Vacuum")); });
    connect(m_ui->vacuumInto, &QPushButton::clicked, this, [this, s] {
        const QString path = QFileDialog::getSaveFileName(
            this, tr("Vacuum Into"), QString(),
            tr("SQLite databases (*.db *.sqlite *.sqlite3);;All files (*)"));
        if (path.isEmpty())
            return;
        // VACUUM INTO will not write over a file that is there.
        if (QFileInfo::exists(path))
            QFile::remove(path);
        run(QStringLiteral("VACUUM %1 INTO %2").arg(s, sqlite::quoteLiteral(path)),
            tr("Vacuum into %1").arg(path));
    });
    connect(m_ui->integrity, &QPushButton::clicked, this, [this, s] {
        run(QStringLiteral("PRAGMA %1.integrity_check").arg(s), tr("Integrity check"));
    });
    connect(m_ui->foreignKeys, &QPushButton::clicked, this, [this] {
        run(QStringLiteral(
                "SELECT \"table\", rowid, parent, fkid FROM pragma_foreign_key_check(NULL, %1)")
                .arg(sqlite::quoteLiteral(m_schema)),
            tr("Foreign key check"));
    });
    connect(m_ui->buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
}

MaintenanceDialog::~MaintenanceDialog() = default;

QString MaintenanceDialog::logText() const
{
    return m_ui->log->toPlainText();
}

QPushButton *MaintenanceDialog::button(const QString &name) const
{
    return findChild<QPushButton *>(name);
}

QStringList MaintenanceDialog::selectedTables() const
{
    QStringList tables;
    for (const QListWidgetItem *item : m_ui->tables->selectedItems())
        tables << item->text();
    return tables;
}

void MaintenanceDialog::forEachTable(const QString &statement, const QString &title)
{
    const QStringList tables = selectedTables();
    if (tables.isEmpty()) {
        run(statement.arg(sql::identifier(m_schema)), title);
        return;
    }
    QStringList statements;
    for (const QString &table : tables)
        statements << statement.arg(sql::identifier(m_schema) + QLatin1Char('.')
                                    + sql::identifier(table));
    run(statements.join(QLatin1String(";\n")), title);
}

void MaintenanceDialog::setBusy(bool busy)
{
    for (QPushButton *b : findChildren<QPushButton *>()) {
        if (m_ui->buttons->buttons().contains(b))
            continue;
        b->setEnabled(!busy);
    }
}

void MaintenanceDialog::run(const QString &sql, const QString &title, std::function<void()> done)
{
    if (!m_session || !m_session->runner()) {
        if (done)
            done();
        return;
    }
    setBusy(true);
    m_ui->log->appendPlainText(
        QStringLiteral("%1 %2…").arg(QTime::currentTime().toString(Qt::ISODate), title));
    m_session->runner()->run(
        sql.toUtf8(), this, [this, title, done](const sqlite::QueryOutcome &o) {
            setBusy(false);
            if (!o.ok()) {
                m_ui->log->appendPlainText(tr("Error: %1").arg(o.error));
            } else {
                int lines = 0;
                for (const sqlite::Result &r : o.results) {
                    for (int row = 0; row < r.rowCount(); ++row) {
                        QStringList cells;
                        for (int c = 0; c < r.columnCount(); ++c)
                            cells << r.valueAt(row, c).toString();
                        m_ui->log->appendPlainText(QLatin1String("    ")
                                                   + cells.join(QLatin1String(" | ")));
                        ++lines;
                    }
                }
                if (lines == 0)
                    m_ui->log->appendPlainText(QLatin1String("    ") + tr("Done."));
            }
            if (done)
                done();
        });
}

} // namespace sqliteman
