// SPDX-FileCopyrightText: 2026 Petr Vanek
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QByteArray>
#include <QObject>
#include <QPointer>
#include <QString>
#include <QStringDecoder>
#include <QStringList>

#include "sqlite/Value.h"

#include <memory>
#include <optional>
#include <vector>

class QIODevice;

namespace sqliteman::sqlite {
class QueryRunner;
struct QueryOutcome;
} // namespace sqliteman::sqlite

namespace sqliteman::catalog {

struct CsvOptions
{
    QChar delimiter = QLatin1Char(',');
    QChar quote = QLatin1Char('"');
    bool header = true; // The first record names the columns.
    int skipLines = 0; // Lines before the header, like a title or a note.
    // An unquoted field that reads exactly this is NULL; a quoted one never
    // is, so "" stays an empty string.
    QString nullText;
    QByteArray encoding = "UTF-8";
};

// One line of the file, or more when a quoted field spans lines.
struct CsvRecord
{
    std::vector<std::optional<QString>> fields; // nullopt for NULL.
};

// Parses CSV text as it arrives, in pieces of any size: RFC 4180, with the
// delimiter and the quote of one's choice, and line breaks inside quotes.
class CsvParser
{
public:
    explicit CsvParser(const CsvOptions &options);

    void feed(QStringView text);
    // The end of the input: a last record with no line break after it.
    void finish();
    std::vector<CsvRecord> take();
    // A quote that was never closed, found by finish().
    bool unterminated() const { return m_unterminated; }

private:
    void endField();
    void endRecord();

    CsvOptions m_options;
    int m_toSkip = 0;
    bool m_inQuotes = false;
    bool m_quoteSeen = false; // In quotes, the last character was a quote.
    bool m_quoted = false; // The current field was quoted.
    bool m_afterCr = false; // A \r ended the last record; a \n may follow.
    bool m_unterminated = false;
    QString m_field;
    CsvRecord m_record;
    bool m_recordStarted = false;
    std::vector<CsvRecord> m_done;
};

// Bytes in a named encoding turned into text, a block at a time. Qt knows
// most encodings only when it is built with ICU, which its builds for
// Windows and macOS are not; the single-byte ones the import offers are
// decoded here instead, so they work the same everywhere.
class TextDecoder
{
public:
    // Any name Qt knows, and those of textEncodings(), in any case.
    explicit TextDecoder(QByteArrayView name);

    bool isValid() const { return m_table || m_qt.isValid(); }
    // Decodes the next block; an empty one at the end flushes what a
    // multi-byte character left unfinished.
    QString decode(QByteArrayView bytes);

private:
    const char16_t *m_table = nullptr; // Bytes 0x80 to 0xFF, for a single-byte encoding.
    QStringDecoder m_qt;
};

// The encodings the import offers, each of which TextDecoder can decode on
// every platform.
QStringList textEncodings();

// A CSV file read a few records at a time, so that a big one is never in
// memory whole.
class CsvFile
{
public:
    CsvFile(QIODevice *device, const CsvOptions &options);
    ~CsvFile();

    // Up to count data records; fewer only at the end of the file.
    std::vector<CsvRecord> read(int count);
    bool atEnd() const;
    // The header's names once read() has run, if options.header.
    QStringList header() const { return m_header; }
    // Bytes read so far, for a progress bar.
    qint64 position() const;
    // Set when the file ends inside a quoted field.
    QString error() const { return m_error; }

private:
    void fill();

    QIODevice *m_device;
    CsvOptions m_options;
    TextDecoder m_decoder;
    CsvParser m_parser;
    std::vector<CsvRecord> m_pending;
    std::size_t m_next = 0;
    bool m_headerRead = false;
    bool m_finished = false;
    QStringList m_header;
    QString m_error;
};

// The delimiter that splits the sample's lines into the same number of
// fields, the most of them; a comma when none does.
QChar guessDelimiter(QStringView sample);
// A column type that fits every value: INTEGER, REAL, or TEXT. NULLs fit
// anything. SQLite has no type for dates; they stay text, which its date
// functions read.
QString guessType(const std::vector<std::optional<QString>> &values);

struct ImportColumn
{
    int source = 0; // The field in each record.
    QString name; // The table's column.
    QString type; // Its declared type: what a field turns into, and for a created table.
};

struct ImportPlan
{
    QString path;
    CsvOptions csv;
    QString schema;
    QString table;
    std::vector<ImportColumn> columns;
    bool create = false; // CREATE TABLE first, with the columns' types.
    bool truncate = false; // Empty the table first.
    int batchRows = 5000; // Records per INSERT batch.
};

QByteArray createTableStatement(const ImportPlan &plan);
// INSERT INTO the plan's table, a parameter per column.
QByteArray insertStatement(const ImportPlan &plan);
// The parameters for each record: a record too short for a column gives
// NULL, and each field becomes what its column's affinity makes of it.
std::vector<std::vector<sqlite::Value>> bindings(const ImportPlan &plan,
                                                 const std::vector<CsvRecord> &records);

// Imports a CSV file into a table in one transaction (a savepoint, if the
// database is in one already): the rows go in as batches of a few thousand
// INSERTs, so a file of any size takes little memory, shows progress and can
// be stopped. If anything fails, nothing is imported.
class ImportJob : public QObject
{
    Q_OBJECT

public:
    // runner must stay alive until finished().
    ImportJob(sqlite::QueryRunner *runner, ImportPlan plan, QObject *parent = nullptr);
    ~ImportJob() override;

    void start();
    // Stops the import and rolls it back; finished() follows.
    void cancel();
    bool isRunning() const { return m_running; }
    qint64 rows() const { return m_rows; }

Q_SIGNALS:
    void progress(qint64 bytesRead, qint64 bytesTotal, qint64 rows);
    // message says what was done, or why nothing was.
    void finished(bool ok, const QString &message);

private:
    enum class Step { Begin, Insert, Commit, Rollback };

    void run(Step step, const QByteArray &sql,
             std::vector<std::vector<sqlite::Value>> parameters = {});
    void onFinished(const sqlite::QueryOutcome &outcome);
    void sendBatch();
    void fail(const QString &message);
    void done(bool ok, const QString &message);

    QPointer<sqlite::QueryRunner> m_runner;
    ImportPlan m_plan;
    std::unique_ptr<QIODevice> m_file;
    std::unique_ptr<CsvFile> m_csv;
    Step m_step = Step::Begin;
    quint64 m_current = 0; // The query running, to cancel it.
    bool m_savepoint = false; // A transaction was open already.
    bool m_running = false;
    bool m_cancelled = false;
    QString m_error;
    qint64 m_rows = 0; // Sent and accepted.
    qint64 m_batchStart = 0; // The first record of the batch running, from 0.
    qint64 m_batchSize = 0;
};

} // namespace sqliteman::catalog
