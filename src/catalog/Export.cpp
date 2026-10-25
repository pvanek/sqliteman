// SPDX-FileCopyrightText: 2026 Petr Vanek
// SPDX-License-Identifier: GPL-3.0-or-later

#include "catalog/Export.h"

#include "sql/Keywords.h"
#include "sqlite/Quote.h"

#include <QCoreApplication>
#include <QTextStream>

#include <algorithm>

namespace sqliteman::catalog {

namespace {

using sqlite::ValueType;

bool isNumber(const sqlite::RowStore &rows, int row, int column)
{
    const ValueType t = rows.type(row, column);
    return t == ValueType::Integer || t == ValueType::Real;
}

std::vector<int> columnsOf(const sqlite::RowStore &rows, const ExportOptions &options)
{
    if (!options.columns.empty())
        return options.columns;
    std::vector<int> all(std::size_t(rows.columnCount()));
    for (int c = 0; c < rows.columnCount(); ++c)
        all[std::size_t(c)] = c;
    return all;
}

// The rows to write, in order: the chosen ones, or all of them, and never
// more than maxRows.
std::vector<int> rowsOf(const sqlite::RowStore &rows, const ExportOptions &options)
{
    std::vector<int> out;
    if (options.rows.empty()) {
        const int last
            = options.maxRows > 0 ? std::min(options.maxRows, rows.rowCount()) : rows.rowCount();
        out.resize(std::size_t(std::max(0, last)));
        for (int row = 0; row < last; ++row)
            out[std::size_t(row)] = row;
        return out;
    }
    for (const int row : options.rows) {
        if (row < 0 || row >= rows.rowCount())
            continue; // A row the user added and has not saved yet.
        if (options.maxRows > 0 && int(out.size()) >= options.maxRows)
            break;
        out.push_back(row);
    }
    return out;
}

// How many rows were left out, for the text view's "… more rows" line.
int skipped(const sqlite::RowStore &rows, const ExportOptions &options, std::size_t written)
{
    const int total = options.rows.empty() ? rows.rowCount() : int(options.rows.size());
    return total - int(written);
}

QString text(const sqlite::RowStore &rows, int row, int column)
{
    return cellText(rows, row, column);
}

// RFC 4180: quote a field holding the delimiter, a quote, a line break or
// spaces at its ends, and double the quotes inside it.
QString csvField(const QString &value, QChar delimiter)
{
    const bool quote = value.contains(delimiter) || value.contains(QLatin1Char('"'))
        || value.contains(QLatin1Char('\n')) || value.contains(QLatin1Char('\r'))
        || (!value.isEmpty() && (value.front().isSpace() || value.back().isSpace()));
    if (!quote)
        return value;
    QString escaped = value;
    escaped.replace(QLatin1Char('"'), QLatin1String("\"\""));
    return QLatin1Char('"') + escaped + QLatin1Char('"');
}

QString cut(const QString &value, int maxWidth)
{
    if (maxWidth <= 0 || value.size() <= maxWidth)
        return value;
    return value.left(std::max(1, maxWidth - 1)) + QChar(0x2026); // …
}

// A cell on one line: control characters would break the table apart.
QString oneLine(const QString &value)
{
    QString out = value;
    out.replace(QLatin1Char('\t'), QLatin1String("    "));
    out.replace(QLatin1Char('\r'), QString());
    out.replace(QLatin1Char('\n'), QLatin1String("\\n"));
    return out;
}

QString literal(const sqlite::RowStore &rows, int row, int column)
{
    return sqlite::literal(rows.valueAt(row, column));
}

QString jsonString(const QString &value)
{
    QString out;
    out.reserve(value.size() + 2);
    out += QLatin1Char('"');
    for (const QChar c : value) {
        switch (c.unicode()) {
        case '"':
            out += QLatin1String("\\\"");
            break;
        case '\\':
            out += QLatin1String("\\\\");
            break;
        case '\n':
            out += QLatin1String("\\n");
            break;
        case '\r':
            out += QLatin1String("\\r");
            break;
        case '\t':
            out += QLatin1String("\\t");
            break;
        default:
            if (c.unicode() < 0x20)
                out += QStringLiteral("\\u%1").arg(int(c.unicode()), 4, 16, QLatin1Char('0'));
            else
                out += c;
        }
    }
    out += QLatin1Char('"');
    return out;
}

QString jsonValue(const sqlite::RowStore &rows, int row, int column)
{
    if (rows.isNull(row, column))
        return QStringLiteral("null");
    const QString value = text(rows, row, column);
    // Inf is a real to SQLite but no number to JSON.
    if (isNumber(rows, row, column) && value.back().isDigit())
        return value;
    return jsonString(value);
}

QString pythonString(const QString &value)
{
    QString out = QStringLiteral("'");
    for (const QChar c : value) {
        switch (c.unicode()) {
        case '\\':
            out += QLatin1String("\\\\");
            break;
        case '\'':
            out += QLatin1String("\\'");
            break;
        case '\n':
            out += QLatin1String("\\n");
            break;
        case '\r':
            out += QLatin1String("\\r");
            break;
        case '\t':
            out += QLatin1String("\\t");
            break;
        default:
            if (c.unicode() < 0x20)
                out += QStringLiteral("\\x%1").arg(int(c.unicode()), 2, 16, QLatin1Char('0'));
            else
                out += c;
        }
    }
    return out + QLatin1Char('\'');
}

QString pythonValue(const sqlite::RowStore &rows, int row, int column)
{
    switch (rows.type(row, column)) {
    case ValueType::Null:
        return QStringLiteral("None");
    case ValueType::Integer:
        return text(rows, row, column);
    case ValueType::Real: {
        const QString value = text(rows, row, column);
        if (value == QLatin1String("Inf"))
            return QStringLiteral("float('inf')");
        if (value == QLatin1String("-Inf"))
            return QStringLiteral("float('-inf')");
        return value;
    }
    case ValueType::Text:
        return pythonString(text(rows, row, column));
    case ValueType::Blob:
        return QStringLiteral("bytes.fromhex('%1')").arg(text(rows, row, column));
    }
    return {};
}

QString markdownCell(const QString &value)
{
    QString out = value;
    out.replace(QLatin1Char('\\'), QLatin1String("\\\\"));
    out.replace(QLatin1Char('|'), QLatin1String("\\|"));
    out.replace(QLatin1String("\r\n"), QLatin1String("<br>"));
    out.replace(QLatin1Char('\n'), QLatin1String("<br>"));
    out.replace(QLatin1Char('\r'), QLatin1String("<br>"));
    return out;
}

// XML 1.0 has no way at all to write most control characters.
QString xmlText(const QString &value)
{
    QString out;
    out.reserve(value.size());
    for (const QChar c : value) {
        switch (c.unicode()) {
        case '&':
            out += QLatin1String("&amp;");
            break;
        case '<':
            out += QLatin1String("&lt;");
            break;
        case '>':
            out += QLatin1String("&gt;");
            break;
        case '"':
            out += QLatin1String("&quot;");
            break;
        case '\t':
        case '\n':
            out += c;
            break;
        case '\r':
            out += QLatin1String("&#13;"); // A parser would turn a bare one into \n.
            break;
        default:
            out += c.unicode() < 0x20 ? QChar(QChar::ReplacementCharacter) : c;
        }
    }
    return out;
}

} // namespace

struct RowWriter::State
{
    State(const ExportOptions &chosen, QTextStream &stream) : options(chosen), out(stream) { }

    ExportOptions options;
    QTextStream &out;
    bool started = false;
    bool finished = false;
    int written = 0;
    // From the first part: the rows of later ones are in the same columns.
    std::vector<int> columns;
    std::vector<QString> names;
    std::vector<bool> numeric;
    std::vector<int> width; // Text: fixed by the first part.

    void begin(const sqlite::RowStore &rows);
    void row(const sqlite::RowStore &rows, int row);
    void end();
    void writeText(const sqlite::RowStore &rows, const std::vector<int> &wanted);
    QString columnList() const;
    QString cell(const sqlite::RowStore &rows, int row, int column, const QString &null) const
    {
        return rows.isNull(row, column) ? null : text(rows, row, column);
    }
};

QString RowWriter::State::columnList() const
{
    QStringList quoted;
    for (const QString &name : names)
        quoted << sql::identifier(name);
    return quoted.join(QStringLiteral(", "));
}

void RowWriter::State::begin(const sqlite::RowStore &rows)
{
    columns = columnsOf(rows, options);
    // SQLite types values, not columns: a column is a number column when
    // every value of the first part that is not NULL is one.
    for (const int column : columns) {
        names.push_back(rows.column(column).name);
        bool any = false;
        bool all = true;
        for (int row = 0; row < rows.rowCount() && all; ++row) {
            if (rows.isNull(row, column))
                continue;
            any = true;
            all = isNumber(rows, row, column);
        }
        numeric.push_back(any && all);
    }
    const qsizetype count = qsizetype(columns.size());

    switch (options.format) {
    case ExportFormat::Csv:
    case ExportFormat::Tsv: {
        if (!options.header)
            break;
        const QChar delimiter
            = options.format == ExportFormat::Tsv ? QLatin1Char('\t') : options.delimiter;
        for (qsizetype i = 0; i < count; ++i)
            out << (i ? QString(delimiter) : QString()) << csvField(names[i], delimiter);
        out << '\n';
        break;
    }
    case ExportFormat::Text:
        break; // Its header needs the widths, which need the rows.
    case ExportFormat::Markdown: {
        // A pipe table cannot do without its header.
        out << '|';
        for (const QString &name : names)
            out << ' ' << markdownCell(name) << " |";
        out << "\n|";
        for (const bool number : numeric)
            out << (number ? "---:|" : "---|");
        out << '\n';
        break;
    }
    case ExportFormat::Html:
        out << "<!DOCTYPE html>\n<html>\n<head>\n<meta charset=\"utf-8\">\n<title>"
            << xmlText(QCoreApplication::translate("sqliteman::catalog", "Sqliteman export"))
            << "</title>\n"
            << "<style>\n"
               "table { border-collapse: collapse; }\n"
               "th, td { border: 1px solid #ccc; padding: 2px 6px; vertical-align: top; "
               "white-space: pre-wrap; }\n"
               "td.number { text-align: right; }\n"
               "</style>\n"
               "</head>\n<body>\n<table>\n";
        if (options.header) {
            out << "<thead>\n<tr>";
            for (const QString &name : names)
                out << "<th>" << xmlText(name) << "</th>";
            out << "</tr>\n</thead>\n";
        }
        out << "<tbody>\n";
        break;
    case ExportFormat::Json:
        out << '[';
        break;
    case ExportFormat::Xml:
        out << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<rows>\n";
        break;
    case ExportFormat::SpreadsheetXml:
        out << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
               "<ss:Workbook xmlns:ss=\"urn:schemas-microsoft-com:office:spreadsheet\">\n"
               "<ss:Styles><ss:Style ss:ID=\"header\"><ss:Font ss:Bold=\"1\"/></ss:Style>"
               "</ss:Styles>\n"
               "<ss:Worksheet ss:Name=\"Sqliteman\">\n<ss:Table>\n";
        if (options.header) {
            out << "<ss:Row ss:StyleID=\"header\">";
            for (const QString &name : names)
                out << "<ss:Cell><ss:Data ss:Type=\"String\">" << xmlText(name)
                    << "</ss:Data></ss:Cell>";
            out << "</ss:Row>\n";
        }
        break;
    case ExportFormat::Python:
        if (options.header) {
            QStringList quoted;
            for (const QString &name : names)
                quoted << pythonString(name);
            out << "# (" << quoted.join(QStringLiteral(", ")) << ")\n";
        }
        out << '[';
        break;
    case ExportFormat::JsonLines:
    case ExportFormat::SqlInsert:
    case ExportFormat::SqlBulkInsert:
        break;
    }
}

void RowWriter::State::row(const sqlite::RowStore &rows, int row)
{
    const qsizetype count = qsizetype(columns.size());
    switch (options.format) {
    case ExportFormat::Csv:
    case ExportFormat::Tsv: {
        const QChar delimiter
            = options.format == ExportFormat::Tsv ? QLatin1Char('\t') : options.delimiter;
        for (qsizetype i = 0; i < count; ++i) {
            const int column = columns[std::size_t(i)];
            if (i)
                out << delimiter;
            out << (rows.isNull(row, column) ? options.nullText
                                             : csvField(text(rows, row, column), delimiter));
        }
        out << '\n';
        break;
    }
    case ExportFormat::Markdown:
        out << '|';
        for (const int column : columns)
            out << ' ' << markdownCell(cell(rows, row, column, options.nullText)) << " |";
        out << '\n';
        break;
    case ExportFormat::Html:
        out << "<tr>";
        for (qsizetype i = 0; i < count; ++i) {
            const int column = columns[std::size_t(i)];
            out << (numeric[std::size_t(i)] ? "<td class=\"number\">" : "<td>")
                << xmlText(cell(rows, row, column, options.nullText)) << "</td>";
        }
        out << "</tr>\n";
        break;
    case ExportFormat::Json:
    case ExportFormat::JsonLines: {
        const bool array = options.format == ExportFormat::Json;
        if (array)
            out << (written ? ",\n  " : "\n  ");
        out << '{';
        for (qsizetype i = 0; i < count; ++i) {
            out << (i ? ", " : "") << jsonString(names[std::size_t(i)]) << ": "
                << jsonValue(rows, row, columns[std::size_t(i)]);
        }
        out << '}';
        if (!array)
            out << '\n';
        break;
    }
    case ExportFormat::Xml:
        out << "  <row>\n";
        for (qsizetype i = 0; i < count; ++i) {
            const int column = columns[std::size_t(i)];
            out << "    <column name=\"" << xmlText(names[std::size_t(i)]) << '"';
            if (rows.isNull(row, column))
                out << " null=\"true\"/>\n";
            else
                out << '>' << xmlText(text(rows, row, column)) << "</column>\n";
        }
        out << "  </row>\n";
        break;
    case ExportFormat::SqlInsert:
    case ExportFormat::SqlBulkInsert: {
        if (columns.empty())
            break;
        QStringList values;
        for (const int column : columns)
            values << literal(rows, row, column);
        const QString tuple
            = QLatin1Char('(') + values.join(QStringLiteral(", ")) + QLatin1Char(')');
        const QString into
            = QStringLiteral("INSERT INTO %1 (%2) VALUES").arg(options.table, columnList());
        if (options.format == ExportFormat::SqlInsert) {
            out << into << ' ' << tuple << ";\n";
            break;
        }
        // The statement before is closed when this row starts a new one, or
        // by end(): only then is it known whether more rows follow.
        const int perStatement = std::max(1, options.bulkRows);
        if (written % perStatement == 0)
            out << (written ? ";\n" : "") << into << '\n';
        else
            out << ",\n";
        out << "    " << tuple;
        break;
    }
    case ExportFormat::SpreadsheetXml:
        out << "<ss:Row>";
        for (const int column : columns) {
            if (rows.isNull(row, column)) {
                out << "<ss:Cell/>";
                continue;
            }
            const bool number
                = isNumber(rows, row, column) && text(rows, row, column).back().isDigit();
            out << "<ss:Cell><ss:Data ss:Type=\"" << (number ? "Number" : "String") << "\">"
                << xmlText(text(rows, row, column)) << "</ss:Data></ss:Cell>";
        }
        out << "</ss:Row>\n";
        break;
    case ExportFormat::Python: {
        QStringList values;
        for (const int column : columns)
            values << pythonValue(rows, row, column);
        // A one-element tuple needs its comma.
        out << "\n    (" << values.join(QStringLiteral(", "))
            << (values.size() == 1 ? ",)," : "),");
        break;
    }
    case ExportFormat::Text:
        break; // writeText() does the whole part at once.
    }
    ++written;
}

void RowWriter::State::writeText(const sqlite::RowStore &rows, const std::vector<int> &wanted)
{
    const QString null = options.nullText.isEmpty() ? QStringLiteral("[NULL]") : options.nullText;
    auto cellText = [&](int row, int column) {
        return cut(rows.isNull(row, column) ? null : oneLine(text(rows, row, column)),
                   options.maxCellWidth);
    };
    auto pad = [](const QString &value, int to, bool right) {
        const QString spaces(std::max(0, to - int(value.size())), QLatin1Char(' '));
        return right ? spaces + value : value + spaces;
    };
    const std::size_t count = columns.size();
    // The last column is not padded: trailing spaces are of no use in a file.
    const std::size_t lastColumn = count - 1;

    // The whole table has to line up, so the widths come first; from the
    // first part alone when the rows stream in, as the header is out by then.
    if (width.empty()) {
        width.resize(count);
        for (std::size_t i = 0; i < count; ++i)
            width[i] = int(cut(names[i], options.maxCellWidth).size());
        for (const int row : wanted)
            for (std::size_t i = 0; i < count; ++i)
                width[i] = std::max(width[i], int(cellText(row, columns[i]).size()));
        if (options.header) {
            for (std::size_t i = 0; i < count; ++i) {
                const QString name = cut(names[i], options.maxCellWidth);
                out << (i ? QStringLiteral(" | ") : QString())
                    << (i == lastColumn ? name : pad(name, width[i], false));
            }
            out << '\n';
            for (std::size_t i = 0; i < count; ++i)
                out << (i ? QStringLiteral("-+-") : QString())
                    << QString(width[i], QLatin1Char('-'));
            out << '\n';
        }
    }
    for (const int row : wanted) {
        for (std::size_t i = 0; i < count; ++i) {
            const int column = columns[i];
            const bool number = numeric[i] && !rows.isNull(row, column);
            const QString value = cellText(row, column);
            out << (i ? QStringLiteral(" | ") : QString())
                << (i == lastColumn && !number ? value : pad(value, width[i], number));
        }
        out << '\n';
        ++written;
    }
    if (const int more = skipped(rows, options, wanted.size()); more > 0)
        out << (more == 1
                    ? QCoreApplication::translate("sqliteman::catalog", "… 1 more row")
                    : QCoreApplication::translate("sqliteman::catalog", "… %1 more rows").arg(more))
            << '\n';
}

void RowWriter::State::end()
{
    switch (options.format) {
    case ExportFormat::Html:
        out << "</tbody>\n</table>\n</body>\n</html>\n";
        break;
    case ExportFormat::Json:
        out << (written ? "\n]\n" : "]\n");
        break;
    case ExportFormat::Xml:
        out << "</rows>\n";
        break;
    case ExportFormat::SqlBulkInsert:
        if (written)
            out << ";\n";
        break;
    case ExportFormat::SpreadsheetXml:
        out << "</ss:Table>\n</ss:Worksheet>\n</ss:Workbook>\n";
        break;
    case ExportFormat::Python:
        out << (written ? "\n]\n" : "]\n");
        break;
    case ExportFormat::Csv:
    case ExportFormat::Tsv:
    case ExportFormat::Text:
    case ExportFormat::Markdown:
    case ExportFormat::JsonLines:
    case ExportFormat::SqlInsert:
        break;
    }
}

RowWriter::RowWriter(const ExportOptions &options, QTextStream &out)
    : m_state(std::make_unique<State>(options, out))
{ }

RowWriter::~RowWriter() = default;

void RowWriter::write(const sqlite::RowStore &rows)
{
    State &s = *m_state;
    if (!rows.hasColumns() || s.finished)
        return;
    if (!s.started) {
        s.begin(rows);
        s.started = true;
    }
    const std::vector<int> wanted = rowsOf(rows, s.options);
    if (s.options.format == ExportFormat::Text) {
        if (!s.columns.empty())
            s.writeText(rows, wanted);
        return;
    }
    for (const int row : wanted)
        s.row(rows, row);
}

void RowWriter::finish()
{
    State &s = *m_state;
    if (s.started && !s.finished)
        s.end();
    s.finished = true;
}

int RowWriter::rowsWritten() const
{
    return m_state->written;
}

QString cellText(const sqlite::RowStore &rows, int row, int column)
{
    const QByteArrayView value = rows.value(row, column);
    if (rows.type(row, column) == ValueType::Blob)
        return QString::fromLatin1(value.toByteArray().toHex());
    return QString::fromUtf8(value);
}

void exportRows(const sqlite::RowStore &rows, const ExportOptions &options, QTextStream &out)
{
    RowWriter writer(options, out);
    writer.write(rows);
    writer.finish();
}

QString exportRows(const sqlite::RowStore &rows, const ExportOptions &options)
{
    QString out;
    QTextStream stream(&out);
    exportRows(rows, options, stream);
    stream.flush();
    return out;
}

QString formatName(ExportFormat format)
{
    switch (format) {
    case ExportFormat::Csv:
        return QCoreApplication::translate("sqliteman::catalog", "CSV");
    case ExportFormat::Tsv:
        return QCoreApplication::translate("sqliteman::catalog", "Tab-separated");
    case ExportFormat::Text:
        return QCoreApplication::translate("sqliteman::catalog", "Text table");
    case ExportFormat::SqlInsert:
        return QCoreApplication::translate("sqliteman::catalog", "SQL INSERT");
    case ExportFormat::SqlBulkInsert:
        return QCoreApplication::translate("sqliteman::catalog", "SQL bulk INSERT");
    case ExportFormat::Json:
        return QCoreApplication::translate("sqliteman::catalog", "JSON");
    case ExportFormat::JsonLines:
        return QCoreApplication::translate("sqliteman::catalog", "JSON Lines");
    case ExportFormat::Markdown:
        return QCoreApplication::translate("sqliteman::catalog", "Markdown");
    case ExportFormat::Html:
        return QCoreApplication::translate("sqliteman::catalog", "HTML");
    case ExportFormat::Xml:
        return QCoreApplication::translate("sqliteman::catalog", "XML");
    case ExportFormat::SpreadsheetXml:
        return QCoreApplication::translate("sqliteman::catalog", "Excel XML");
    case ExportFormat::Python:
        return QCoreApplication::translate("sqliteman::catalog", "Python list");
    }
    return {};
}

QString fileSuffix(ExportFormat format)
{
    switch (format) {
    case ExportFormat::Csv:
        return QStringLiteral("csv");
    case ExportFormat::Tsv:
        return QStringLiteral("tsv");
    case ExportFormat::Text:
        return QStringLiteral("txt");
    case ExportFormat::SqlInsert:
    case ExportFormat::SqlBulkInsert:
        return QStringLiteral("sql");
    case ExportFormat::SpreadsheetXml:
        return QStringLiteral("xml");
    case ExportFormat::Python:
        return QStringLiteral("py");
    case ExportFormat::Json:
        return QStringLiteral("json");
    case ExportFormat::JsonLines:
        return QStringLiteral("jsonl");
    case ExportFormat::Markdown:
        return QStringLiteral("md");
    case ExportFormat::Html:
        return QStringLiteral("html");
    case ExportFormat::Xml:
        return QStringLiteral("xml");
    }
    return QStringLiteral("txt");
}

QString fileFilter(ExportFormat format)
{
    return QStringLiteral("%1 (*.%2)").arg(formatName(format), fileSuffix(format));
}

} // namespace sqliteman::catalog
