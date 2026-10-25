// SPDX-FileCopyrightText: 2026 Petr Vanek
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "catalog/Import.h"

#include <QDialog>
#include <QPointer>

#include <memory>

namespace Ui {
class ImportDialog;
}

namespace sqliteman {

class Session;

// Imports a CSV file into a table, or into a new one made to fit it: which
// file columns go to which table columns, a preview, and the import itself
// with its progress.
class ImportDialog : public QDialog
{
    Q_OBJECT

public:
    // Into schema.table when table is set; otherwise into a new table in
    // schema. session may be null, in tests.
    ImportDialog(Session *session, const QString &schema, const QString &table,
                 QWidget *parent = nullptr);
    ~ImportDialog() override;

    // Reads the file's first rows for the preview and the columns, and
    // guesses its delimiter.
    void setPath(const QString &path);
    // The existing table's columns. The dialog takes them from the schema
    // the session read; tests set them.
    void setTableColumns(const QStringList &names, const QStringList &types);
    catalog::ImportPlan plan() const;
    bool isRunning() const;
    QString statusText() const;

Q_SIGNALS:
    // Rows went in, and a table may have been made.
    void imported(const QString &schema, const QString &table);

private:
    void browse();
    void readSample();
    void fillColumns();
    void updateType(int row);
    void updateEnabled();
    void loadTableColumns();
    void startImport();
    void setStatus(const QString &text, bool error = false);
    void finish(bool ok, const QString &message);
    catalog::CsvOptions csvOptions() const;
    int fileColumnCount() const;
    void reject() override;

    std::unique_ptr<Ui::ImportDialog> m_ui; // Layout in ImportDialog.ui.
    QPointer<Session> m_session;
    QStringList m_header;
    std::vector<catalog::CsvRecord> m_sample;
    QString m_sampleError;
    QStringList m_tableColumns;
    QStringList m_tableTypes;
    QString m_loadedTable; // Whose columns m_tableColumns are.
    catalog::ImportJob *m_job = nullptr;
};

} // namespace sqliteman
