// SPDX-FileCopyrightText: 2026 Petr Vanek
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "sqlite/RowStore.h"

#include <QString>

#include <memory>
#include <vector>

class QTextStream;

namespace sqliteman::catalog {

enum class ExportFormat {
    Csv,
    Tsv,
    Text, // The ASCII table the result's text view shows.
    SqlInsert, // One INSERT per row.
    SqlBulkInsert, // One INSERT for every few rows.
    Json, // An array of objects, one per row.
    JsonLines, // One object per line, for tools that read a row at a time.
    Markdown, // A pipe table.
    Html, // A page with a table, which spreadsheets open too.
    Xml,
    SpreadsheetXml, // Excel 2003 XML, which spreadsheets open with a column per column.
    Python // A list of tuples, to paste into a script.
};

struct ExportOptions
{
    ExportFormat format = ExportFormat::Csv;
    bool header = true; // CSV, TSV, text and HTML.
    QChar delimiter = QLatin1Char(','); // CSV; TSV always uses a tab.
    QString nullText; // What a NULL is written as; JSON, XML, SQL and Python have their own.
    QString table = QStringLiteral("table_name"); // INSERT INTO this, as it goes into SQL.
    int bulkRows = 100; // Rows per bulk INSERT.
    int maxCellWidth = 0; // Text: cut longer cells; 0 keeps them.
    int maxRows = 0; // 0 exports every row.
    std::vector<int> rows; // Which rows, in this order; empty exports all.
    std::vector<int> columns; // Empty exports every column.
};

// Writes rows that arrive in parts, as a query streams them, into one
// document: the header or an opening bracket go before the first part and
// what closes them after the last. options.rows and maxRows
// apply to each part on its own.
class RowWriter
{
public:
    RowWriter(const ExportOptions &options, QTextStream &out);
    ~RowWriter();
    RowWriter(const RowWriter &) = delete;
    RowWriter &operator=(const RowWriter &) = delete;

    // The columns are taken from the first part that has any.
    void write(const sqlite::RowStore &rows);
    // Closes what the first part opened; nothing if no part had columns.
    void finish();
    int rowsWritten() const;

private:
    struct State;
    std::unique_ptr<State> m_state;
};

// Writes the rows in the chosen format. Streaming, so a big result needs no
// big string in memory.
void exportRows(const sqlite::RowStore &rows, const ExportOptions &options, QTextStream &out);
QString exportRows(const sqlite::RowStore &rows, const ExportOptions &options);

// For save dialogs and menus.
QString formatName(ExportFormat format);
QString fileFilter(ExportFormat format);
QString fileSuffix(ExportFormat format);

// A cell as text: numbers and text as they are, a blob in hex digits.
QString cellText(const sqlite::RowStore &rows, int row, int column);

} // namespace sqliteman::catalog
