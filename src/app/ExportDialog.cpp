// SPDX-FileCopyrightText: 2026 Petr Vanek
// SPDX-License-Identifier: GPL-3.0-or-later

#include "ExportDialog.h"

#include "ui_ExportDialog.h"

#include <QFileDialog>
#include <QFileInfo>
#include <QMessageBox>
#include <QPushButton>

#include <algorithm>
#include <array>

namespace sqliteman {

using catalog::ExportFormat;

namespace {

constexpr std::array Formats {ExportFormat::Csv,           ExportFormat::Tsv,
                              ExportFormat::Json,          ExportFormat::JsonLines,
                              ExportFormat::Markdown,      ExportFormat::Html,
                              ExportFormat::Xml,           ExportFormat::SpreadsheetXml,
                              ExportFormat::Text,          ExportFormat::SqlInsert,
                              ExportFormat::SqlBulkInsert, ExportFormat::Python};

ExportFormat lastFormat = ExportFormat::Csv;

} // namespace

ExportDialog::ExportDialog(const Counts &counts, const QString &table, QWidget *parent)
    : QDialog(parent), m_ui(std::make_unique<Ui::ExportDialog>()), m_counts(counts)
{
    m_ui->setupUi(this);

    m_ui->rows->addItem(tr("Fetched rows (%1)").arg(counts.fetched), int(Scope::Fetched));
    m_ui->rows->addItem(tr("Selected rows (%1)").arg(counts.selected), int(Scope::Selected));
    m_ui->rows->addItem(counts.truncated ? tr("All rows (runs the query again)")
                                         : tr("All rows (%1)").arg(counts.fetched),
                        int(Scope::All));
    if (counts.selected > 0)
        m_ui->rows->setCurrentIndex(1); // What is selected is usually what is meant.

    for (const ExportFormat format : Formats)
        m_ui->format->addItem(catalog::formatName(format), int(format));
    m_ui->format->setCurrentIndex(
        int(std::distance(Formats.begin(), std::find(Formats.begin(), Formats.end(), lastFormat))));
    if (!table.isEmpty())
        m_ui->table->setText(table);

    connect(m_ui->buttons, &QDialogButtonBox::accepted, this, &ExportDialog::accept);
    connect(m_ui->buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(m_ui->format, &QComboBox::currentIndexChanged, this, [this] { updateEnabled(); });
    connect(m_ui->rows, &QComboBox::currentIndexChanged, this, [this] { updateEnabled(); });
    connect(m_ui->toFile, &QRadioButton::toggled, this, [this] { updateEnabled(); });
    connect(m_ui->browse, &QPushButton::clicked, this, &ExportDialog::browse);
    updateEnabled();
}

ExportDialog::~ExportDialog() = default;

ExportDialog::Scope ExportDialog::scope() const
{
    return Scope(m_ui->rows->currentData().toInt());
}

ExportFormat ExportDialog::format() const
{
    return ExportFormat(m_ui->format->currentData().toInt());
}

catalog::ExportOptions ExportDialog::options() const
{
    catalog::ExportOptions options;
    options.format = format();
    options.header = m_ui->header->isChecked();
    if (const QString delimiter = m_ui->delimiter->text(); !delimiter.isEmpty())
        options.delimiter = delimiter.front();
    options.nullText = m_ui->nullText->text();
    if (!m_ui->table->text().isEmpty())
        options.table = m_ui->table->text();
    options.bulkRows = m_ui->bulkRows->value();
    return options;
}

bool ExportDialog::toClipboard() const
{
    return m_ui->toClipboard->isChecked();
}

QString ExportDialog::path() const
{
    return m_ui->path->text();
}

void ExportDialog::setLastFormat(ExportFormat format)
{
    lastFormat = format;
}

void ExportDialog::updateEnabled()
{
    const ExportFormat chosen = format();
    auto is = [chosen](std::initializer_list<ExportFormat> formats) {
        return std::ranges::find(formats, chosen) != formats.end();
    };
    const bool sql = is({ExportFormat::SqlInsert, ExportFormat::SqlBulkInsert});

    // A Markdown table needs its header; JSON and XML name every value.
    m_ui->header->setEnabled(
        is({ExportFormat::Csv, ExportFormat::Tsv, ExportFormat::Text, ExportFormat::Html,
            ExportFormat::SpreadsheetXml, ExportFormat::Python}));
    m_ui->delimiterLabel->setEnabled(chosen == ExportFormat::Csv);
    m_ui->delimiter->setEnabled(chosen == ExportFormat::Csv);
    // These have a NULL of their own.
    const bool ownNull = sql
        || is({ExportFormat::Json, ExportFormat::JsonLines, ExportFormat::Xml,
               ExportFormat::SpreadsheetXml, ExportFormat::Python});
    m_ui->nullLabel->setEnabled(!ownNull);
    m_ui->nullText->setEnabled(!ownNull);
    m_ui->tableLabel->setEnabled(sql);
    m_ui->table->setEnabled(sql);
    m_ui->bulkLabel->setEnabled(chosen == ExportFormat::SqlBulkInsert);
    m_ui->bulkRows->setEnabled(chosen == ExportFormat::SqlBulkInsert);

    const bool file = m_ui->toFile->isChecked();
    m_ui->path->setEnabled(file);
    m_ui->browse->setEnabled(file);

    // Saying what the choice means beats greying it out with no reason given.
    QString note;
    if (scope() == Scope::Selected && m_counts.selected == 0)
        note = tr("No rows are selected.");
    else if (scope() == Scope::All && m_counts.truncated)
        note = tr("The query was stopped at %1 rows; all of them means running it "
                  "again and writing the rows as they arrive.")
                   .arg(m_counts.fetched);
    else if (scope() == Scope::All && m_counts.truncated && toClipboard())
        note = tr("Only fetched rows can be copied to the clipboard.");
    m_ui->note->setText(note);

    const bool nothing = scope() == Scope::Selected && m_counts.selected == 0;
    if (QPushButton *ok = m_ui->buttons->button(QDialogButtonBox::Ok))
        ok->setEnabled(!nothing);
}

void ExportDialog::browse()
{
    const ExportFormat chosen = format();
    QString suggested = m_ui->path->text();
    if (suggested.isEmpty())
        suggested = tr("result") + QLatin1Char('.') + catalog::fileSuffix(chosen);
    const QString chosenPath = QFileDialog::getSaveFileName(this, tr("Export Rows"), suggested,
                                                            catalog::fileFilter(chosen));
    if (!chosenPath.isEmpty())
        m_ui->path->setText(chosenPath);
}

void ExportDialog::accept()
{
    if (!toClipboard() && path().isEmpty()) {
        browse();
        if (path().isEmpty())
            return;
    }
    setLastFormat(format());
    QDialog::accept();
}

} // namespace sqliteman
