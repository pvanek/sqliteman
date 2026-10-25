// SPDX-FileCopyrightText: 2026 Petr Vanek
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "catalog/Export.h"

#include <QDialog>

#include <memory>

namespace Ui {
class ExportDialog;
}

namespace sqliteman {

// What to write out, in which format, and where to.
class ExportDialog : public QDialog
{
    Q_OBJECT

public:
    // Which rows the export covers.
    enum class Scope {
        Fetched, // The rows that were fetched, as the grid has them.
        Selected, // The rows selected in the grid.
        All // Every row there is: runs the query again if it was cut short.
    };

    struct Counts
    {
        int fetched = 0;
        int selected = 0;
        bool truncated = false; // More rows on the server than were fetched.
    };

    ExportDialog(const Counts &counts, const QString &table, QWidget *parent = nullptr);
    ~ExportDialog() override;

    Scope scope() const;
    catalog::ExportFormat format() const;
    catalog::ExportOptions options() const;
    bool toClipboard() const;
    QString path() const;

    // Remembers what was chosen last, for the next time the dialog opens.
    static void setLastFormat(catalog::ExportFormat format);

private:
    void updateEnabled();
    void browse();
    void accept() override;

    std::unique_ptr<Ui::ExportDialog> m_ui; // Layout in ExportDialog.ui.
    Counts m_counts;
};

} // namespace sqliteman
