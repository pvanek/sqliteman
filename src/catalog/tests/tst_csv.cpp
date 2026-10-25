// SPDX-FileCopyrightText: 2026 Petr Vanek
// SPDX-License-Identifier: GPL-3.0-or-later

#include "catalog/Import.h"

#include <QBuffer>
#include <QTest>

using namespace sqliteman::catalog;

namespace {

using Fields = std::vector<std::optional<QString>>;

std::vector<CsvRecord> parse(const QString &text, const CsvOptions &options = {})
{
    CsvParser parser(options);
    parser.feed(text);
    parser.finish();
    return parser.take();
}

Fields field(std::initializer_list<const char *> values)
{
    Fields out;
    for (const char *v : values)
        out.emplace_back(v ? std::optional<QString>(QString::fromUtf8(v)) : std::nullopt);
    return out;
}

} // namespace

class TestCsv : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void parsesQuotesAndBreaks()
    {
        const auto records = parse(QStringLiteral("a,\"b, c\",\"say \"\"hi\"\"\"\r\n"
                                                  "1,\"two\nlines\",\n"
                                                  "\n"
                                                  "x,\"\",y"));
        QCOMPARE(records.size(), std::size_t(3));
        QCOMPARE(records[0].fields, field({"a", "b, c", "say \"hi\""}));
        // An unquoted empty field is NULL, a quoted one an empty string.
        QCOMPARE(records[1].fields, field({"1", "two\nlines", nullptr}));
        QCOMPARE(records[2].fields, field({"x", "", "y"}));
    }

    // The file arrives in blocks that may cut anything in two, a doubled
    // quote and a \r\n included.
    void parsesInPieces()
    {
        const QString text = QStringLiteral("\"a\"\"b\",c\r\nd,e\r\n");
        for (qsizetype cut = 1; cut < text.size(); ++cut) {
            CsvParser parser({});
            parser.feed(QStringView(text).left(cut));
            parser.feed(QStringView(text).mid(cut));
            parser.finish();
            const auto records = parser.take();
            QCOMPARE(records.size(), std::size_t(2));
            QCOMPARE(records[0].fields, field({"a\"b", "c"}));
            QCOMPARE(records[1].fields, field({"d", "e"}));
        }
    }

    void options()
    {
        CsvOptions options;
        options.delimiter = QLatin1Char(';');
        options.quote = QLatin1Char('\'');
        options.nullText = QStringLiteral("NULL");
        options.skipLines = 1;
        const auto records
            = parse(QStringLiteral("Exported 2026-10-21\n'a;b';NULL;'NULL';\n"), options);
        QCOMPARE(records.size(), std::size_t(1));
        // With a NULL text, empty fields are empty strings.
        QCOMPARE(records[0].fields, field({"a;b", nullptr, "NULL", ""}));
    }

    void unterminatedQuote()
    {
        CsvParser parser({});
        parser.feed(QStringLiteral("a,\"b\n"));
        parser.finish();
        QVERIFY(parser.unterminated());
        QCOMPARE(parser.take().size(), std::size_t(1));
    }

    void readsAFile()
    {
        QByteArray bytes = "\xEF\xBB\xBF"
                           "id,name\n"; // With a byte order mark.
        for (int i = 1; i <= 10; ++i)
            bytes += QByteArray::number(i) + ",n\xC3\xA1zev " + QByteArray::number(i) + '\n';
        QBuffer buffer(&bytes);
        QVERIFY(buffer.open(QIODevice::ReadOnly));
        CsvFile file(&buffer, {});
        const auto first = file.read(4);
        QCOMPARE(file.header(), (QStringList {QStringLiteral("id"), QStringLiteral("name")}));
        QCOMPARE(first.size(), std::size_t(4));
        QCOMPARE(first[0].fields, field({"1", "název 1"}));
        QVERIFY(!file.atEnd());
        QCOMPARE(file.read(100).size(), std::size_t(6));
        QVERIFY(file.atEnd());
        QVERIFY(file.read(100).empty());
        QVERIFY(file.error().isEmpty());
        QCOMPARE(file.position(), qint64(bytes.size()));
    }

    void readsOtherEncodings()
    {
        QByteArray bytes = "a\n\xE8\x61j\n"; // "čaj" in Windows-1250.
        QBuffer buffer(&bytes);
        QVERIFY(buffer.open(QIODevice::ReadOnly));
        CsvOptions options;
        options.encoding = "windows-1250";
        CsvFile file(&buffer, options);
        const auto records = file.read(10);
        QCOMPARE(records.size(), std::size_t(1));
        QCOMPARE(records[0].fields, field({"čaj"}));
    }

    // What the import offers decodes on every platform, ICU or not: Qt's
    // builds for Windows and macOS have none.
    void decodesEveryEncodingOffered()
    {
        const QStringList names = textEncodings();
        QVERIFY(names.contains(QStringLiteral("windows-1250")));
        for (const QString &name : names)
            QVERIFY2(TextDecoder(name.toLatin1()).isValid(), qPrintable(name));
        QVERIFY(!TextDecoder("no-such-encoding").isValid());

        const auto decoded = [](const char *name, const char *bytes) {
            return TextDecoder(name).decode(QByteArrayView(bytes));
        };
        QCOMPARE(decoded("windows-1250", "\xE8\x61j"), QStringLiteral("čaj"));
        QCOMPARE(decoded("WINDOWS-1250", "\x9A\x9E"), QStringLiteral("šž")); // Any case.
        QCOMPARE(decoded("windows-1251", "\xC4\xE0"), QStringLiteral("Да"));
        QCOMPARE(decoded("windows-1252", "\x80 caf\xE9"), QStringLiteral("€ café"));
        QCOMPARE(decoded("ISO-8859-2", "\xB9\xE8"), QStringLiteral("šč"));
        // A byte the encoding leaves undefined is the replacement character.
        QCOMPARE(decoded("windows-1252", "\x81"), QString(QChar::ReplacementCharacter));
    }

    // The tables are the program's own: where Qt has ICU, it must agree with
    // them on every byte they define.
    void tablesAgreeWithICU()
    {
        QByteArray all;
        for (int b = 0; b < 256; ++b)
            all += char(b);
        int compared = 0;
        for (const char *name : {"ISO-8859-2", "windows-1250", "windows-1251", "windows-1252"}) {
            QStringDecoder icu(name);
            if (!icu.isValid())
                continue;
            ++compared;
            const QString ours = TextDecoder(name).decode(all);
            const QString theirs = icu.decode(all);
            QCOMPARE(ours.size(), theirs.size());
            for (qsizetype i = 0; i < ours.size(); ++i) {
                if (ours[i] != QChar::ReplacementCharacter)
                    QVERIFY2(
                        ours[i] == theirs[i],
                        qPrintable(QStringLiteral("%1, byte 0x%2: U+%3, ICU U+%4")
                                       .arg(QLatin1String(name))
                                       .arg(i, 2, 16, QLatin1Char('0'))
                                       .arg(int(ours[i].unicode()), 4, 16, QLatin1Char('0'))
                                       .arg(int(theirs[i].unicode()), 4, 16, QLatin1Char('0'))));
            }
        }
        if (compared == 0)
            QSKIP("Qt has no ICU here to compare the tables with");
    }

    void guessesTheDelimiter()
    {
        QCOMPARE(guessDelimiter(u"a,b,c\n1,2,3\n"), QLatin1Char(','));
        QCOMPARE(guessDelimiter(u"a;b;c\n1,5;2;3\n4;5;6"), QLatin1Char(';'));
        QCOMPARE(guessDelimiter(u"a\tb\n\"x,y\"\t2\n"), QLatin1Char('\t'));
        QCOMPARE(guessDelimiter(u"a|b\n1|2\n"), QLatin1Char('|'));
        QCOMPARE(guessDelimiter(u"just text\n"), QLatin1Char(','));
    }

    void guessesTypes()
    {
        QCOMPARE(guessType(field({"1", "-20", nullptr})), QStringLiteral("INTEGER"));
        QCOMPARE(guessType(field({"1", "9000000000"})), QStringLiteral("INTEGER"));
        QCOMPARE(guessType(field({"1", "2.5", "1e3"})), QStringLiteral("REAL"));
        // Beyond 64 bits it does not fit an integer.
        QCOMPARE(guessType(field({"99999999999999999999"})), QStringLiteral("REAL"));
        // SQLite has no date type; its date functions read text.
        QCOMPARE(guessType(field({"2026-10-21", "2024-02-29"})), QStringLiteral("TEXT"));
        // An empty string goes into no number.
        QCOMPARE(guessType(field({"1", ""})), QStringLiteral("TEXT"));
        QCOMPARE(guessType(field({nullptr})), QStringLiteral("TEXT"));
    }

    void buildsStatements()
    {
        ImportPlan plan;
        plan.schema = QStringLiteral("main");
        plan.table = QStringLiteral("My Table");
        plan.columns = {{1, QStringLiteral("id"), QStringLiteral("INTEGER")},
                        {0, QStringLiteral("when"), QStringLiteral("TEXT")},
                        {2, QStringLiteral("x"), QString()}};
        QCOMPARE(
            createTableStatement(plan),
            QByteArray(
                "CREATE TABLE main.\"My Table\" (\n    id INTEGER,\n    \"when\" TEXT,\n    x\n)"));
        QCOMPARE(insertStatement(plan),
                 QByteArray("INSERT INTO main.\"My Table\" (id, \"when\", x) VALUES (?, ?, ?)"));

        std::vector<CsvRecord> records(2);
        records[0].fields = field({"a", "1", "2.5"});
        records[1].fields = field({nullptr, "x"}); // Short: the rest are NULL.
        const auto values = bindings(plan, records);
        using sqliteman::sqlite::Value;
        QCOMPARE(values[0],
                 (std::vector<Value> {Value::integer(1), Value::text(QStringLiteral("a")),
                                      Value::real(2.5)}));
        // Text a number column cannot take stays text, as SQLite would keep it.
        QCOMPARE(values[1],
                 (std::vector<Value> {Value::text(QStringLiteral("x")), Value(), Value()}));
    }
};

QTEST_GUILESS_MAIN(TestCsv)
#include "tst_csv.moc"
