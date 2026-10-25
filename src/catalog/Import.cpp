// SPDX-FileCopyrightText: 2026 Petr Vanek
// SPDX-License-Identifier: GPL-3.0-or-later

#include "catalog/Import.h"

#include "catalog/Editing.h"
#include "catalog/Snapshot.h"
#include "sql/Keywords.h"
#include "sqlite/QueryRunner.h"

#include <QCoreApplication>
#include <QFile>
#include <QRegularExpression>

#include <sqlite3.h>

namespace sqliteman::catalog {

namespace {

QString tr(const char *text)
{
    return QCoreApplication::translate("sqliteman::catalog", text);
}

// windows-1250, bytes 0x80 to 0xFF.
constexpr char16_t Cp1250[128] = {
    0x20AC, 0xFFFD, 0x201A, 0xFFFD, 0x201E, 0x2026, 0x2020, 0x2021, 0xFFFD, 0x2030, 0x0160, 0x2039,
    0x015A, 0x0164, 0x017D, 0x0179, 0xFFFD, 0x2018, 0x2019, 0x201C, 0x201D, 0x2022, 0x2013, 0x2014,
    0xFFFD, 0x2122, 0x0161, 0x203A, 0x015B, 0x0165, 0x017E, 0x017A, 0x00A0, 0x02C7, 0x02D8, 0x0141,
    0x00A4, 0x0104, 0x00A6, 0x00A7, 0x00A8, 0x00A9, 0x015E, 0x00AB, 0x00AC, 0x00AD, 0x00AE, 0x017B,
    0x00B0, 0x00B1, 0x02DB, 0x0142, 0x00B4, 0x00B5, 0x00B6, 0x00B7, 0x00B8, 0x0105, 0x015F, 0x00BB,
    0x013D, 0x02DD, 0x013E, 0x017C, 0x0154, 0x00C1, 0x00C2, 0x0102, 0x00C4, 0x0139, 0x0106, 0x00C7,
    0x010C, 0x00C9, 0x0118, 0x00CB, 0x011A, 0x00CD, 0x00CE, 0x010E, 0x0110, 0x0143, 0x0147, 0x00D3,
    0x00D4, 0x0150, 0x00D6, 0x00D7, 0x0158, 0x016E, 0x00DA, 0x0170, 0x00DC, 0x00DD, 0x0162, 0x00DF,
    0x0155, 0x00E1, 0x00E2, 0x0103, 0x00E4, 0x013A, 0x0107, 0x00E7, 0x010D, 0x00E9, 0x0119, 0x00EB,
    0x011B, 0x00ED, 0x00EE, 0x010F, 0x0111, 0x0144, 0x0148, 0x00F3, 0x00F4, 0x0151, 0x00F6, 0x00F7,
    0x0159, 0x016F, 0x00FA, 0x0171, 0x00FC, 0x00FD, 0x0163, 0x02D9,
};
// windows-1251, bytes 0x80 to 0xFF.
constexpr char16_t Cp1251[128] = {
    0x0402, 0x0403, 0x201A, 0x0453, 0x201E, 0x2026, 0x2020, 0x2021, 0x20AC, 0x2030, 0x0409, 0x2039,
    0x040A, 0x040C, 0x040B, 0x040F, 0x0452, 0x2018, 0x2019, 0x201C, 0x201D, 0x2022, 0x2013, 0x2014,
    0xFFFD, 0x2122, 0x0459, 0x203A, 0x045A, 0x045C, 0x045B, 0x045F, 0x00A0, 0x040E, 0x045E, 0x0408,
    0x00A4, 0x0490, 0x00A6, 0x00A7, 0x0401, 0x00A9, 0x0404, 0x00AB, 0x00AC, 0x00AD, 0x00AE, 0x0407,
    0x00B0, 0x00B1, 0x0406, 0x0456, 0x0491, 0x00B5, 0x00B6, 0x00B7, 0x0451, 0x2116, 0x0454, 0x00BB,
    0x0458, 0x0405, 0x0455, 0x0457, 0x0410, 0x0411, 0x0412, 0x0413, 0x0414, 0x0415, 0x0416, 0x0417,
    0x0418, 0x0419, 0x041A, 0x041B, 0x041C, 0x041D, 0x041E, 0x041F, 0x0420, 0x0421, 0x0422, 0x0423,
    0x0424, 0x0425, 0x0426, 0x0427, 0x0428, 0x0429, 0x042A, 0x042B, 0x042C, 0x042D, 0x042E, 0x042F,
    0x0430, 0x0431, 0x0432, 0x0433, 0x0434, 0x0435, 0x0436, 0x0437, 0x0438, 0x0439, 0x043A, 0x043B,
    0x043C, 0x043D, 0x043E, 0x043F, 0x0440, 0x0441, 0x0442, 0x0443, 0x0444, 0x0445, 0x0446, 0x0447,
    0x0448, 0x0449, 0x044A, 0x044B, 0x044C, 0x044D, 0x044E, 0x044F,
};
// windows-1252, bytes 0x80 to 0xFF.
constexpr char16_t Cp1252[128] = {
    0x20AC, 0xFFFD, 0x201A, 0x0192, 0x201E, 0x2026, 0x2020, 0x2021, 0x02C6, 0x2030, 0x0160, 0x2039,
    0x0152, 0xFFFD, 0x017D, 0xFFFD, 0xFFFD, 0x2018, 0x2019, 0x201C, 0x201D, 0x2022, 0x2013, 0x2014,
    0x02DC, 0x2122, 0x0161, 0x203A, 0x0153, 0xFFFD, 0x017E, 0x0178, 0x00A0, 0x00A1, 0x00A2, 0x00A3,
    0x00A4, 0x00A5, 0x00A6, 0x00A7, 0x00A8, 0x00A9, 0x00AA, 0x00AB, 0x00AC, 0x00AD, 0x00AE, 0x00AF,
    0x00B0, 0x00B1, 0x00B2, 0x00B3, 0x00B4, 0x00B5, 0x00B6, 0x00B7, 0x00B8, 0x00B9, 0x00BA, 0x00BB,
    0x00BC, 0x00BD, 0x00BE, 0x00BF, 0x00C0, 0x00C1, 0x00C2, 0x00C3, 0x00C4, 0x00C5, 0x00C6, 0x00C7,
    0x00C8, 0x00C9, 0x00CA, 0x00CB, 0x00CC, 0x00CD, 0x00CE, 0x00CF, 0x00D0, 0x00D1, 0x00D2, 0x00D3,
    0x00D4, 0x00D5, 0x00D6, 0x00D7, 0x00D8, 0x00D9, 0x00DA, 0x00DB, 0x00DC, 0x00DD, 0x00DE, 0x00DF,
    0x00E0, 0x00E1, 0x00E2, 0x00E3, 0x00E4, 0x00E5, 0x00E6, 0x00E7, 0x00E8, 0x00E9, 0x00EA, 0x00EB,
    0x00EC, 0x00ED, 0x00EE, 0x00EF, 0x00F0, 0x00F1, 0x00F2, 0x00F3, 0x00F4, 0x00F5, 0x00F6, 0x00F7,
    0x00F8, 0x00F9, 0x00FA, 0x00FB, 0x00FC, 0x00FD, 0x00FE, 0x00FF,
};
// ISO-8859-2, bytes 0x80 to 0xFF.
constexpr char16_t Iso8859_2[128] = {
    0x0080, 0x0081, 0x0082, 0x0083, 0x0084, 0x0085, 0x0086, 0x0087, 0x0088, 0x0089, 0x008A, 0x008B,
    0x008C, 0x008D, 0x008E, 0x008F, 0x0090, 0x0091, 0x0092, 0x0093, 0x0094, 0x0095, 0x0096, 0x0097,
    0x0098, 0x0099, 0x009A, 0x009B, 0x009C, 0x009D, 0x009E, 0x009F, 0x00A0, 0x0104, 0x02D8, 0x0141,
    0x00A4, 0x013D, 0x015A, 0x00A7, 0x00A8, 0x0160, 0x015E, 0x0164, 0x0179, 0x00AD, 0x017D, 0x017B,
    0x00B0, 0x0105, 0x02DB, 0x0142, 0x00B4, 0x013E, 0x015B, 0x02C7, 0x00B8, 0x0161, 0x015F, 0x0165,
    0x017A, 0x02DD, 0x017E, 0x017C, 0x0154, 0x00C1, 0x00C2, 0x0102, 0x00C4, 0x0139, 0x0106, 0x00C7,
    0x010C, 0x00C9, 0x0118, 0x00CB, 0x011A, 0x00CD, 0x00CE, 0x010E, 0x0110, 0x0143, 0x0147, 0x00D3,
    0x00D4, 0x0150, 0x00D6, 0x00D7, 0x0158, 0x016E, 0x00DA, 0x0170, 0x00DC, 0x00DD, 0x0162, 0x00DF,
    0x0155, 0x00E1, 0x00E2, 0x0103, 0x00E4, 0x013A, 0x0107, 0x00E7, 0x010D, 0x00E9, 0x0119, 0x00EB,
    0x011B, 0x00ED, 0x00EE, 0x010F, 0x0111, 0x0144, 0x0148, 0x00F3, 0x00F4, 0x0151, 0x00F6, 0x00F7,
    0x0159, 0x016F, 0x00FA, 0x0171, 0x00FC, 0x00FD, 0x0163, 0x02D9,
};

struct SingleByte
{
    QLatin1StringView name;
    const char16_t *table;
};

// Generated from Python's codecs; a byte an encoding leaves undefined is
// U+FFFD, the replacement character.
constexpr SingleByte SingleByteEncodings[] = {
    {QLatin1StringView("ISO-8859-2"), Iso8859_2},
    {QLatin1StringView("windows-1250"), Cp1250},
    {QLatin1StringView("windows-1251"), Cp1251},
    {QLatin1StringView("windows-1252"), Cp1252},
};

QString qualified(const QString &schema, const QString &table)
{
    return schema.isEmpty() ? sql::identifier(table)
                            : sql::identifier(schema) + QLatin1Char('.') + sql::identifier(table);
}

constexpr qint64 BlockSize = 64 * 1024;

} // namespace

// CsvParser.

CsvParser::CsvParser(const CsvOptions &options)
    : m_options(options), m_toSkip(options.skipLines) { }

void CsvParser::feed(QStringView text)
{
    const QChar quote = m_options.quote;
    const QChar delimiter = m_options.delimiter;
    for (const QChar c : text) {
        if (m_toSkip > 0) {
            if (c == QLatin1Char('\n'))
                --m_toSkip;
            continue;
        }
        if (m_afterCr) {
            m_afterCr = false;
            if (c == QLatin1Char('\n'))
                continue;
        }
        if (m_inQuotes) {
            if (!m_quoteSeen) {
                if (c == quote)
                    m_quoteSeen = true;
                else
                    m_field += c;
                continue;
            }
            m_quoteSeen = false;
            if (c == quote) { // Doubled: a quote of the value's own.
                m_field += c;
                continue;
            }
            m_inQuotes = false; // The quote before closed the field.
        }
        if (c == delimiter) {
            endField();
            m_recordStarted = true;
        } else if (c == QLatin1Char('\n') || c == QLatin1Char('\r')) {
            // Blank lines are no records: a file often ends with one or two.
            if (m_recordStarted || !m_field.isEmpty())
                endRecord();
            m_afterCr = c == QLatin1Char('\r');
        } else if (c == quote && m_field.isEmpty() && !m_quoted) {
            m_inQuotes = true;
            m_quoted = true;
            m_recordStarted = true;
        } else {
            m_field += c;
            m_recordStarted = true;
        }
    }
}

void CsvParser::finish()
{
    if (m_inQuotes && !m_quoteSeen)
        m_unterminated = true;
    m_inQuotes = false;
    m_quoteSeen = false;
    if (m_recordStarted || !m_field.isEmpty())
        endRecord();
}

std::vector<CsvRecord> CsvParser::take()
{
    return std::exchange(m_done, {});
}

void CsvParser::endField()
{
    if (!m_quoted && m_field == m_options.nullText)
        m_record.fields.emplace_back(std::nullopt);
    else
        m_record.fields.emplace_back(m_field);
    m_field.clear();
    m_quoted = false;
}

void CsvParser::endRecord()
{
    endField();
    m_done.push_back(std::move(m_record));
    m_record = {};
    m_recordStarted = false;
}

// TextDecoder.

TextDecoder::TextDecoder(QByteArrayView name)
{
    const QString wanted = QString::fromLatin1(name);
    for (const SingleByte &encoding : SingleByteEncodings) {
        if (wanted.compare(encoding.name, Qt::CaseInsensitive) == 0) {
            m_table = encoding.table;
            return;
        }
    }
    m_qt = QStringDecoder(QString::fromLatin1(name));
}

QString TextDecoder::decode(QByteArrayView bytes)
{
    if (!m_table)
        return m_qt.decode(bytes);
    QString text(bytes.size(), Qt::Uninitialized);
    QChar *out = text.data();
    for (const char byte : bytes) {
        const auto b = static_cast<unsigned char>(byte);
        *out++ = b < 0x80 ? QChar(char16_t(b)) : QChar(m_table[b - 0x80]);
    }
    return text;
}

QStringList textEncodings()
{
    QStringList names {QStringLiteral("UTF-8"), QStringLiteral("UTF-16"),
                       QStringLiteral("ISO-8859-1")};
    for (const SingleByte &encoding : SingleByteEncodings)
        names << encoding.name;
    return names;
}

// CsvFile.

CsvFile::CsvFile(QIODevice *device, const CsvOptions &options)
    : m_device(device), m_options(options), m_decoder(options.encoding), m_parser(options),
      m_headerRead(!options.header)
{
    if (!m_decoder.isValid())
        m_decoder = TextDecoder("UTF-8");
}

CsvFile::~CsvFile() = default;

void CsvFile::fill()
{
    const QByteArray bytes = m_device->read(BlockSize);
    if (bytes.isEmpty()) {
        m_parser.feed(m_decoder.decode(QByteArrayView()));
        m_parser.finish();
        m_finished = true;
        if (m_parser.unterminated())
            m_error = tr("The file ends inside a quoted field: a quote is not closed.");
    } else {
        m_parser.feed(m_decoder.decode(bytes));
    }
    for (CsvRecord &record : m_parser.take())
        m_pending.push_back(std::move(record));
}

std::vector<CsvRecord> CsvFile::read(int count)
{
    while (!m_headerRead) {
        if (m_next < m_pending.size()) {
            for (const std::optional<QString> &name : m_pending[m_next].fields)
                m_header << name.value_or(QString()).trimmed();
            ++m_next;
            m_headerRead = true;
        } else if (m_finished) {
            m_headerRead = true;
        } else {
            fill();
        }
    }
    std::vector<CsvRecord> out;
    while (int(out.size()) < count) {
        if (m_next < m_pending.size()) {
            out.push_back(std::move(m_pending[m_next++]));
            continue;
        }
        m_pending.clear();
        m_next = 0;
        if (m_finished)
            break;
        fill();
    }
    return out;
}

bool CsvFile::atEnd() const
{
    return m_finished && m_headerRead && m_next >= m_pending.size();
}

qint64 CsvFile::position() const
{
    return m_device->pos();
}

// Guessing.

QChar guessDelimiter(QStringView sample)
{
    // The last line may be cut off in the middle.
    const qsizetype lastBreak = sample.lastIndexOf(QLatin1Char('\n'));
    const QStringView whole = lastBreak > 0 ? sample.left(lastBreak + 1) : sample;
    QChar best = QLatin1Char(',');
    std::size_t bestFields = 1;
    for (const QChar candidate :
         {QLatin1Char(','), QLatin1Char(';'), QLatin1Char('\t'), QLatin1Char('|')}) {
        CsvOptions options;
        options.delimiter = candidate;
        CsvParser parser(options);
        parser.feed(whole);
        parser.finish();
        const std::vector<CsvRecord> records = parser.take();
        if (records.empty())
            continue;
        const std::size_t fields = records.front().fields.size();
        const bool even = std::ranges::all_of(
            records, [fields](const CsvRecord &r) { return r.fields.size() == fields; });
        if (even && fields > bestFields) {
            best = candidate;
            bestFields = fields;
        }
    }
    return best;
}

QString guessType(const std::vector<std::optional<QString>> &values)
{
    static const QRegularExpression integer(QStringLiteral("^[+-]?\\d+$"));
    static const QRegularExpression number(
        QStringLiteral("^[+-]?(\\d+\\.?\\d*|\\.\\d+)([eE][+-]?\\d+)?$"));

    bool any = false;
    bool isInteger = true;
    bool isNumber = true;
    for (const std::optional<QString> &value : values) {
        if (!value)
            continue;
        any = true;
        const QString v = value->trimmed();
        if (integer.match(v).hasMatch()) {
            bool ok = false;
            v.toLongLong(&ok);
            isInteger = isInteger && ok; // Beyond 64 bits it is a real.
        } else {
            isInteger = false;
        }
        isNumber = isNumber && number.match(v).hasMatch();
    }
    if (!any)
        return QStringLiteral("TEXT");
    if (isInteger)
        return QStringLiteral("INTEGER");
    if (isNumber)
        return QStringLiteral("REAL");
    return QStringLiteral("TEXT");
}

// Statements.

QByteArray createTableStatement(const ImportPlan &plan)
{
    QStringList columns;
    for (const ImportColumn &column : plan.columns) {
        QString definition = QLatin1String("    ") + sql::identifier(column.name);
        if (!column.type.isEmpty())
            definition += QLatin1Char(' ') + column.type;
        columns << definition;
    }
    return QStringLiteral("CREATE TABLE %1 (\n%2\n)")
        .arg(qualified(plan.schema, plan.table), columns.join(QStringLiteral(",\n")))
        .toUtf8();
}

QByteArray insertStatement(const ImportPlan &plan)
{
    QStringList names;
    QStringList parameters;
    for (const ImportColumn &column : plan.columns) {
        names << sql::identifier(column.name);
        parameters << QStringLiteral("?");
    }
    return QStringLiteral("INSERT INTO %1 (%2) VALUES (%3)")
        .arg(qualified(plan.schema, plan.table), names.join(QStringLiteral(", ")),
             parameters.join(QStringLiteral(", ")))
        .toUtf8();
}

std::vector<std::vector<sqlite::Value>> bindings(const ImportPlan &plan,
                                                 const std::vector<CsvRecord> &records)
{
    std::vector<Affinity> affinities;
    for (const ImportColumn &column : plan.columns)
        affinities.push_back(affinity(column.type));
    std::vector<std::vector<sqlite::Value>> out;
    out.reserve(records.size());
    for (const CsvRecord &record : records) {
        std::vector<sqlite::Value> row;
        row.reserve(plan.columns.size());
        for (std::size_t i = 0; i < plan.columns.size(); ++i) {
            const auto field = std::size_t(plan.columns[i].source);
            if (field < record.fields.size() && record.fields[field])
                row.push_back(typedValue(*record.fields[field], affinities[i]));
            else
                row.push_back(sqlite::Value());
        }
        out.push_back(std::move(row));
    }
    return out;
}

// ImportJob.

ImportJob::ImportJob(sqlite::QueryRunner *runner, ImportPlan plan, QObject *parent)
    : QObject(parent), m_runner(runner), m_plan(std::move(plan))
{ }

ImportJob::~ImportJob() = default;

void ImportJob::start()
{
    if (m_running)
        return;
    m_running = true;
    m_cancelled = false;
    m_error.clear();
    m_rows = 0;

    auto file = std::make_unique<QFile>(m_plan.path);
    if (!file->open(QIODevice::ReadOnly)) {
        done(false, tr("Could not read %1: %2").arg(m_plan.path, file->errorString()));
        return;
    }
    m_file = std::move(file);
    m_csv = std::make_unique<CsvFile>(m_file.get(), m_plan.csv);
    if (!m_runner || !m_runner->connection()) {
        done(false, tr("The database is not open."));
        return;
    }
    // A savepoint nests in a transaction the user has open, and undoes only
    // the import if it fails.
    m_savepoint = m_runner->connection()->inTransaction();
    QByteArray sql = m_savepoint ? "SAVEPOINT sqliteman_import" : "BEGIN";
    if (m_plan.create)
        sql += ";\n" + createTableStatement(m_plan);
    if (m_plan.truncate && !m_plan.create)
        sql += ";\nDELETE FROM " + qualified(m_plan.schema, m_plan.table).toUtf8();
    run(Step::Begin, sql);
}

void ImportJob::cancel()
{
    if (!m_running || m_cancelled)
        return;
    m_cancelled = true;
    // The running statement ends with an error, and the rollback follows.
    if (m_runner && m_step != Step::Rollback && m_current)
        m_runner->cancel(m_current);
}

void ImportJob::run(Step step, const QByteArray &sql,
                    std::vector<std::vector<sqlite::Value>> parameters)
{
    m_step = step;
    if (!m_runner) {
        done(false, tr("The database is not open."));
        return;
    }
    sqlite::ExecuteOptions options;
    options.bindings = std::move(parameters);
    m_current = m_runner->run(
        sql, this, [this](const sqlite::QueryOutcome &outcome) { onFinished(outcome); }, options);
}

void ImportJob::onFinished(const sqlite::QueryOutcome &outcome)
{
    m_current = 0;
    if (!m_running)
        return;
    if (!outcome.ok() && m_step != Step::Rollback && m_error.isEmpty() && !m_cancelled) {
        int failed = -1;
        for (const sqlite::Result &r : outcome.results)
            if (r.isError())
                failed = r.failedBinding();
        if (m_step == Step::Insert && failed >= 0)
            m_error = tr("Row %1 of the file's data: %2")
                          .arg(m_batchStart + failed + 1)
                          .arg(outcome.error);
        else
            m_error = outcome.error;
    }
    if (m_step == Step::Rollback) {
        done(false,
             m_cancelled && m_error.isEmpty()
                 ? tr("The import was stopped; nothing was imported.")
                 : m_error + QLatin1Char('\n') + tr("Nothing was imported."));
        return;
    }
    if (!m_error.isEmpty() || m_cancelled) {
        run(Step::Rollback,
            m_savepoint ? "ROLLBACK TO sqliteman_import; RELEASE sqliteman_import" : "ROLLBACK");
        return;
    }
    switch (m_step) {
    case Step::Begin:
        sendBatch();
        break;
    case Step::Insert:
        m_rows += m_batchSize;
        Q_EMIT progress(m_csv->position(), m_file->size(), m_rows);
        sendBatch();
        break;
    case Step::Commit:
        done(true,
             tr("Imported %1 rows into %2.")
                 .arg(m_rows)
                 .arg(qualified(m_plan.schema, m_plan.table)));
        break;
    case Step::Rollback:
        break;
    }
}

void ImportJob::sendBatch()
{
    const std::vector<CsvRecord> records = m_csv->read(std::max(1, m_plan.batchRows));
    if (!m_csv->error().isEmpty()) {
        fail(m_csv->error());
        return;
    }
    if (records.empty()) {
        run(Step::Commit, m_savepoint ? "RELEASE sqliteman_import" : "COMMIT");
        return;
    }
    m_batchStart = m_rows;
    m_batchSize = qint64(records.size());
    run(Step::Insert, insertStatement(m_plan), bindings(m_plan, records));
}

void ImportJob::fail(const QString &message)
{
    m_error = message;
    run(Step::Rollback,
        m_savepoint ? "ROLLBACK TO sqliteman_import; RELEASE sqliteman_import" : "ROLLBACK");
}

void ImportJob::done(bool ok, const QString &message)
{
    if (!m_running)
        return;
    m_running = false;
    m_csv.reset();
    m_file.reset();
    Q_EMIT finished(ok, message);
}

} // namespace sqliteman::catalog
