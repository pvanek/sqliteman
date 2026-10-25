// SPDX-FileCopyrightText: 2026 Petr Vanek
// SPDX-License-Identifier: GPL-3.0-or-later

#include "catalog/Export.h"

#include <QTest>
#include <QTextStream>

#include <limits>

using namespace sqliteman::catalog;
using sqliteman::sqlite::Column;
using sqliteman::sqlite::Result;
using sqliteman::sqlite::RowStore;
using sqliteman::sqlite::Value;

namespace {

// A result built in memory, every value with the storage class it has.
Result makeResult(std::initializer_list<const char *> columns,
                  std::initializer_list<std::initializer_list<Value>> rows)
{
    auto data = std::make_shared<Result::Data>();
    auto names = std::make_shared<std::vector<Column>>();
    for (const char *name : columns)
        names->push_back({QString::fromUtf8(name), {}, {}, {}, {}});
    data->columns = names;
    data->rows.columns = int(names->size());
    for (const auto &row : rows) {
        for (const Value &v : row)
            data->rows.add(v.type(), v.data());
        ++data->rows.rows;
    }
    return Result(std::move(data));
}

Value text(const char *s)
{
    return Value::text(QString::fromUtf8(s));
}

RowStore store()
{
    RowStore rows;
    rows.append(makeResult({"id", "name", "note"},
                           {{Value::integer(1), text("Alice"), text("plain")},
                            {Value::integer(2), text("Bo, \"the\" Builder"), Value()},
                            {Value::integer(42), text("line\nbreak"), text("it's")}}));
    return rows;
}

ExportOptions as(ExportFormat format)
{
    ExportOptions options;
    options.format = format;
    return options;
}

} // namespace

class TestExport : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void csv()
    {
        ExportOptions options;
        options.format = ExportFormat::Csv;
        QCOMPARE(exportRows(store(), options),
                 QStringLiteral("id,name,note\n"
                                "1,Alice,plain\n"
                                "2,\"Bo, \"\"the\"\" Builder\",\n"
                                "42,\"line\nbreak\",it's\n"));

        options.header = false;
        options.nullText = QStringLiteral("NULL");
        options.columns = {0, 2};
        QCOMPARE(exportRows(store(), options),
                 QStringLiteral("1,plain\n"
                                "2,NULL\n"
                                "42,it's\n"));
    }

    void tsv()
    {
        ExportOptions options;
        options.format = ExportFormat::Tsv;
        options.maxRows = 1;
        QCOMPARE(exportRows(store(), options), QStringLiteral("id\tname\tnote\n1\tAlice\tplain\n"));
    }

    void textTable()
    {
        ExportOptions options;
        options.format = ExportFormat::Text;
        // Numbers right, text left, NULLs marked, line breaks kept on one line.
        QCOMPARE(exportRows(store(), options),
                 QStringLiteral("id | name              | note\n"
                                "---+-------------------+-------\n"
                                " 1 | Alice             | plain\n"
                                " 2 | Bo, \"the\" Builder | [NULL]\n"
                                "42 | line\\nbreak       | it's\n"));
    }

    void textTableCutsWideCells()
    {
        ExportOptions options;
        options.format = ExportFormat::Text;
        options.maxCellWidth = 6;
        options.maxRows = 2;
        QCOMPARE(exportRows(store(), options),
                 QStringLiteral("id | name   | note\n"
                                "---+--------+-------\n"
                                " 1 | Alice  | plain\n"
                                " 2 | Bo, \"… | [NULL]\n"
                                "… 1 more row\n"));
    }

    void sqlInserts()
    {
        ExportOptions options;
        options.format = ExportFormat::SqlInsert;
        options.table = QStringLiteral("people");
        options.maxRows = 2;
        // Numbers unquoted, quotes doubled, NULL as NULL.
        QCOMPARE(exportRows(store(), options),
                 QStringLiteral("INSERT INTO people (id, name, note) VALUES "
                                "(1, 'Alice', 'plain');\n"
                                "INSERT INTO people (id, name, note) VALUES "
                                "(2, 'Bo, \"the\" Builder', NULL);\n"));
    }

    void sqlBulkInserts()
    {
        ExportOptions options;
        options.format = ExportFormat::SqlBulkInsert;
        options.table = QStringLiteral("people");
        options.bulkRows = 2;
        options.columns = {0, 1};
        QCOMPARE(exportRows(store(), options),
                 QStringLiteral("INSERT INTO people (id, name) VALUES\n"
                                "    (1, 'Alice'),\n"
                                "    (2, 'Bo, \"the\" Builder');\n"
                                "INSERT INTO people (id, name) VALUES\n"
                                "    (42, 'line\nbreak');\n"));
    }

    void valuesOfEveryStorageClass()
    {
        RowStore rows;
        rows.append(
            makeResult({"Mixed Case", "r", "b"},
                       {{text("12"), Value::real(1.5), Value::blob(QByteArray("\x00\xff", 2))},
                        {Value::integer(7), Value::real(std::numeric_limits<double>::infinity()),
                         Value::blob(QByteArray())}}));
        ExportOptions options;
        options.format = ExportFormat::SqlInsert;
        // Text that reads as a number stays text; a blob is a blob literal.
        QCOMPARE(exportRows(rows, options),
                 QStringLiteral(
                     "INSERT INTO table_name (\"Mixed Case\", r, b) VALUES ('12', 1.5, X'00FF');\n"
                     "INSERT INTO table_name (\"Mixed Case\", r, b) VALUES (7, 9e999, X'');\n"));
        // Elsewhere a blob is written in hex digits.
        QCOMPARE(exportRows(rows, as(ExportFormat::Csv)),
                 QStringLiteral("Mixed Case,r,b\n12,1.5,00ff\n7,Inf,\n"));
        QCOMPARE(exportRows(rows, as(ExportFormat::JsonLines)),
                 QStringLiteral("{\"Mixed Case\": \"12\", \"r\": 1.5, \"b\": \"00ff\"}\n"
                                "{\"Mixed Case\": 7, \"r\": \"Inf\", \"b\": \"\"}\n"));
    }

    void emptyResult()
    {
        RowStore rows;
        ExportOptions options;
        QVERIFY(exportRows(rows, options).isEmpty());
        rows.append(makeResult({"id"}, {}));
        options.format = ExportFormat::Csv;
        QCOMPARE(exportRows(rows, options), QStringLiteral("id\n"));
    }

    void json()
    {
        RowStore rows;
        rows.append(makeResult({"id", "doc", "x"},
                               {{Value::integer(1), text("{\"a\": [1]}"), Value::real(-1.5)},
                                {Value::integer(2), Value(), Value::real(2)}}));
        // SQLite keeps JSON as text: it is written as a string.
        QCOMPARE(exportRows(rows, as(ExportFormat::Json)),
                 QStringLiteral("[\n"
                                "  {\"id\": 1, \"doc\": \"{\\\"a\\\": [1]}\", \"x\": -1.5},\n"
                                "  {\"id\": 2, \"doc\": null, \"x\": 2.0}\n"
                                "]\n"));
        RowStore empty;
        empty.append(makeResult({"id"}, {}));
        QCOMPARE(exportRows(empty, as(ExportFormat::Json)), QStringLiteral("[]\n"));
    }

    void markdown()
    {
        RowStore rows;
        rows.append(makeResult(
            {"id", "a|b"}, {{Value::integer(1), text("x|y\nz")}, {Value::integer(2), Value()}}));
        ExportOptions options;
        options.format = ExportFormat::Markdown;
        options.nullText = QStringLiteral("NULL");
        QCOMPARE(exportRows(rows, options),
                 QStringLiteral("| id | a\\|b |\n"
                                "|---:|---|\n"
                                "| 1 | x\\|y<br>z |\n"
                                "| 2 | NULL |\n"));
    }

    void html()
    {
        RowStore rows;
        rows.append(makeResult({"id", "<b>"}, {{Value::integer(1), text("a & b")}}));
        const QString out = exportRows(rows, as(ExportFormat::Html));
        QVERIFY(out.startsWith(QLatin1String("<!DOCTYPE html>")));
        QVERIFY(out.contains(QLatin1String("<tr><th>id</th><th>&lt;b&gt;</th></tr>")));
        QVERIFY(
            out.contains(QLatin1String("<tr><td class=\"number\">1</td><td>a &amp; b</td></tr>")));
        QVERIFY(out.endsWith(QLatin1String("</table>\n</body>\n</html>\n")));
    }

    void xml()
    {
        RowStore rows;
        rows.append(makeResult(
            {"id", "note"}, {{Value::integer(1), text("a<b\x01")}, {Value::integer(2), Value()}}));
        QCOMPARE(exportRows(rows, as(ExportFormat::Xml)),
                 QStringLiteral("<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
                                "<rows>\n"
                                "  <row>\n"
                                "    <column name=\"id\">1</column>\n"
                                "    <column name=\"note\">a&lt;b�</column>\n"
                                "  </row>\n"
                                "  <row>\n"
                                "    <column name=\"id\">2</column>\n"
                                "    <column name=\"note\" null=\"true\"/>\n"
                                "  </row>\n"
                                "</rows>\n"));
    }

    void spreadsheet()
    {
        RowStore rows;
        rows.append(makeResult({"id", "name"},
                               {{Value::integer(1), text("a&b")}, {Value::integer(2), Value()}}));
        const QString out = exportRows(rows, as(ExportFormat::SpreadsheetXml));
        QVERIFY(out.startsWith(QLatin1String("<?xml")));
        QVERIFY(out.contains(QLatin1String("<ss:Row ss:StyleID=\"header\"><ss:Cell><ss:Data "
                                           "ss:Type=\"String\">id</ss:Data></ss:Cell>")));
        QVERIFY(out.contains(QLatin1String(
            "<ss:Row><ss:Cell><ss:Data ss:Type=\"Number\">1</ss:Data></ss:Cell>"
            "<ss:Cell><ss:Data ss:Type=\"String\">a&amp;b</ss:Data></ss:Cell></ss:Row>")));
        QVERIFY(out.contains(QLatin1String("<ss:Cell/>")));
        QVERIFY(out.endsWith(QLatin1String("</ss:Workbook>\n")));
    }

    void python()
    {
        RowStore rows;
        rows.append(
            makeResult({"id", "s", "b"},
                       {{Value::integer(1), text("it's\n"), Value::blob(QByteArray("\x01", 1))},
                        {Value(), Value::real(0.5), Value()}}));
        QCOMPARE(exportRows(rows, as(ExportFormat::Python)),
                 QStringLiteral("# ('id', 's', 'b')\n"
                                "[\n"
                                "    (1, 'it\\'s\\n', bytes.fromhex('01')),\n"
                                "    (None, 0.5, None),\n"
                                "]\n"));
        RowStore single;
        single.append(makeResult({"x"}, {{Value::integer(1)}}));
        ExportOptions options = as(ExportFormat::Python);
        options.header = false;
        QCOMPARE(exportRows(single, options), QStringLiteral("[\n    (1,),\n]\n"));
    }

    // A query's rows arrive in parts; the document around them is one.
    void writesPartsAsOne()
    {
        RowStore first;
        first.append(makeResult({"id"}, {{Value::integer(1)}, {Value::integer(2)}}));
        RowStore second;
        second.append(makeResult({"id"}, {{Value::integer(3)}}));
        auto parts = [&](const ExportOptions &options) {
            QString out;
            QTextStream stream(&out);
            RowWriter writer(options, stream);
            writer.write(first);
            writer.write(second);
            writer.finish();
            stream.flush();
            return out;
        };
        QCOMPARE(parts(as(ExportFormat::Csv)), QStringLiteral("id\n1\n2\n3\n"));
        QCOMPARE(parts(as(ExportFormat::Json)),
                 QStringLiteral("[\n  {\"id\": 1},\n  {\"id\": 2},\n  {\"id\": 3}\n]\n"));
        ExportOptions bulk = as(ExportFormat::SqlBulkInsert);
        bulk.table = QStringLiteral("t");
        bulk.bulkRows = 2;
        QCOMPARE(parts(bulk),
                 QStringLiteral("INSERT INTO t (id) VALUES\n    (1),\n    (2);\n"
                                "INSERT INTO t (id) VALUES\n    (3);\n"));
        QCOMPARE(parts(as(ExportFormat::Text)), QStringLiteral("id\n--\n 1\n 2\n 3\n"));
    }

    void namesAndSuffixes()
    {
        QCOMPARE(fileSuffix(ExportFormat::Csv), QStringLiteral("csv"));
        QCOMPARE(fileSuffix(ExportFormat::SqlBulkInsert), QStringLiteral("sql"));
        QVERIFY(fileFilter(ExportFormat::Text).contains(QLatin1String("*.txt")));
        QVERIFY(!formatName(ExportFormat::SqlInsert).isEmpty());
        QCOMPARE(fileSuffix(ExportFormat::SpreadsheetXml), QStringLiteral("xml"));
        QCOMPARE(fileSuffix(ExportFormat::Python), QStringLiteral("py"));
    }
};

QTEST_GUILESS_MAIN(TestExport)
#include "tst_export.moc"
