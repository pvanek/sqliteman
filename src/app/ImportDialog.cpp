// SPDX-FileCopyrightText: 2026 Petr Vanek
// SPDX-License-Identifier: GPL-3.0-or-later

#include "ImportDialog.h"

#include "Session.h"

#include "ui_ImportDialog.h"

#include <QComboBox>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QPushButton>

#include <set>

namespace sqliteman {

using catalog::CsvRecord;

namespace {

enum ColumnsColumn { FileColumn, FirstValue, TableColumn, TypeColumn };

constexpr int SampleRows = 200;

const QStringList Types {QStringLiteral("TEXT"), QStringLiteral("INTEGER"), QStringLiteral("REAL"),
                         QStringLiteral("NUMERIC"), QStringLiteral("BLOB")};

// How names compare when matching file columns to table columns: "First
// Name" goes into first_name.
QString loose(const QString &name)
{
    QString out = name.simplified().toLower();
    out.replace(QLatin1Char(' '), QLatin1Char('_'));
    out.replace(QLatin1Char('-'), QLatin1Char('_'));
    return out;
}

QString shortened(const QString &value)
{
    QString out = value;
    out.replace(QLatin1Char('\n'), QLatin1Char(' '));
    return out.size() > 60 ? out.left(59) + QChar(0x2026) : out;
}

} // namespace

ImportDialog::ImportDialog(Session *session, const QString &schema, const QString &table,
                           QWidget *parent)
    : QDialog(parent), m_ui(std::make_unique<Ui::ImportDialog>()), m_session(session)
{
    m_ui->setupUi(this);
    m_ui->buttons->button(QDialogButtonBox::Ok)->setText(tr("&Import"));
    m_ui->progress->hide(); // Until there is progress to show.

    m_ui->delimiter->addItem(tr("Comma"), QStringLiteral(","));
    m_ui->delimiter->addItem(tr("Semicolon"), QStringLiteral(";"));
    m_ui->delimiter->addItem(tr("Tab"), QStringLiteral("\t"));
    m_ui->delimiter->addItem(tr("Pipe"), QStringLiteral("|"));
    m_ui->encoding->addItems(catalog::textEncodings());

    m_ui->schema->setText(schema.isEmpty() ? QStringLiteral("main") : schema);
    m_ui->table->setText(table);
    m_ui->newTable->setChecked(table.isEmpty());
    QHeaderView *header = m_ui->columns->horizontalHeader();
    header->setSectionResizeMode(FileColumn, QHeaderView::ResizeToContents);
    header->setSectionResizeMode(FirstValue, QHeaderView::Interactive);
    header->resizeSection(FirstValue, 200);
    header->setSectionResizeMode(TableColumn, QHeaderView::Interactive);
    header->resizeSection(TableColumn, 180);

    connect(m_ui->browse, &QPushButton::clicked, this, &ImportDialog::browse);
    connect(m_ui->path, &QLineEdit::editingFinished, this, [this] {
        if (m_ui->path->isModified()) {
            m_ui->path->setModified(false);
            setPath(m_ui->path->text());
        }
    });
    auto reread = [this] { readSample(); };
    connect(m_ui->delimiter, &QComboBox::currentIndexChanged, this, reread);
    connect(m_ui->quote, &QLineEdit::textChanged, this, reread);
    connect(m_ui->encoding, &QComboBox::currentTextChanged, this, reread);
    connect(m_ui->skipLines, &QSpinBox::valueChanged, this, reread);
    connect(m_ui->nullText, &QLineEdit::textChanged, this, reread);
    connect(m_ui->header, &QCheckBox::toggled, this, reread);
    connect(m_ui->newTable, &QRadioButton::toggled, this, [this] {
        fillColumns();
        loadTableColumns();
    });
    connect(m_ui->schema, &QLineEdit::editingFinished, this, &ImportDialog::loadTableColumns);
    connect(m_ui->table, &QLineEdit::editingFinished, this, &ImportDialog::loadTableColumns);
    connect(m_ui->table, &QLineEdit::textChanged, this, &ImportDialog::updateEnabled);
    connect(m_ui->columns, &QTableWidget::itemChanged, this, &ImportDialog::updateEnabled);
    connect(m_ui->buttons, &QDialogButtonBox::accepted, this, &ImportDialog::startImport);
    connect(m_ui->buttons, &QDialogButtonBox::rejected, this, &ImportDialog::reject);

    loadTableColumns();
    updateEnabled();
}

ImportDialog::~ImportDialog()
{
    if (m_job)
        m_job->disconnect(this);
}

void ImportDialog::setPath(const QString &path)
{
    m_ui->path->setText(path);
    // The delimiter is what makes the most even columns of the first lines.
    QFile file(path);
    if (file.open(QIODevice::ReadOnly)) {
        catalog::TextDecoder decoder(m_ui->encoding->currentText().toLatin1());
        if (!decoder.isValid())
            decoder = catalog::TextDecoder("UTF-8");
        const QString sample = decoder.decode(file.read(64 * 1024));
        const QChar delimiter = catalog::guessDelimiter(sample);
        const QSignalBlocker blocker(m_ui->delimiter);
        if (const int index = m_ui->delimiter->findData(QString(delimiter)); index >= 0)
            m_ui->delimiter->setCurrentIndex(index);
    }
    if (m_ui->newTable->isChecked() && m_ui->table->text().isEmpty())
        m_ui->table->setText(loose(QFileInfo(path).completeBaseName()));
    readSample();
}

void ImportDialog::setTableColumns(const QStringList &names, const QStringList &types)
{
    m_tableColumns = names;
    m_tableTypes = types;
    fillColumns();
}

catalog::CsvOptions ImportDialog::csvOptions() const
{
    catalog::CsvOptions options;
    options.delimiter = m_ui->delimiter->currentData().toString().front();
    if (!m_ui->quote->text().isEmpty())
        options.quote = m_ui->quote->text().front();
    options.encoding = m_ui->encoding->currentText().toUtf8();
    options.header = m_ui->header->isChecked();
    options.skipLines = m_ui->skipLines->value();
    options.nullText = m_ui->nullText->text();
    return options;
}

catalog::ImportPlan ImportDialog::plan() const
{
    catalog::ImportPlan plan;
    plan.path = m_ui->path->text();
    plan.csv = csvOptions();
    plan.schema = m_ui->schema->text().trimmed();
    plan.table = m_ui->table->text().trimmed();
    plan.create = m_ui->newTable->isChecked();
    plan.truncate = !plan.create && m_ui->truncate->isChecked();
    for (int row = 0; row < m_ui->columns->rowCount(); ++row) {
        const QTableWidgetItem *use = m_ui->columns->item(row, FileColumn);
        if (!use || use->checkState() != Qt::Checked)
            continue;
        catalog::ImportColumn column;
        column.source = row;
        if (plan.create) {
            column.name = m_ui->columns->item(row, TableColumn)->text().trimmed();
            if (auto *type = qobject_cast<QComboBox *>(m_ui->columns->cellWidget(row, TypeColumn)))
                column.type = type->currentText().trimmed();
        } else if (auto *target
                   = qobject_cast<QComboBox *>(m_ui->columns->cellWidget(row, TableColumn))) {
            column.name = target->currentText();
        }
        if (!column.name.isEmpty())
            plan.columns.push_back(column);
    }
    return plan;
}

bool ImportDialog::isRunning() const
{
    return m_job;
}

QString ImportDialog::statusText() const
{
    return m_ui->status->text();
}

void ImportDialog::browse()
{
    const QString path
        = QFileDialog::getOpenFileName(this, tr("Import CSV"), m_ui->path->text(),
                                       tr("CSV files (*.csv *.tsv *.txt);;All files (*)"));
    if (!path.isEmpty())
        setPath(path);
}

int ImportDialog::fileColumnCount() const
{
    qsizetype count = m_header.size();
    for (const CsvRecord &record : m_sample)
        count = std::max(count, qsizetype(record.fields.size()));
    return int(count);
}

void ImportDialog::readSample()
{
    m_header.clear();
    m_sample.clear();
    m_sampleError.clear();
    const QString path = m_ui->path->text();
    if (!path.isEmpty()) {
        QFile file(path);
        if (file.open(QIODevice::ReadOnly)) {
            catalog::CsvFile csv(&file, csvOptions());
            m_sample = csv.read(SampleRows);
            m_header = csv.header();
            m_sampleError = csv.error();
        } else {
            m_sampleError = tr("Could not read %1: %2").arg(path, file.errorString());
        }
    }

    const int count = fileColumnCount();
    QTableWidget *preview = m_ui->preview;
    preview->clear();
    preview->setColumnCount(count);
    preview->setRowCount(int(m_sample.size()));
    QStringList names;
    for (int c = 0; c < count; ++c)
        names << (m_header.value(c).isEmpty() ? tr("Column %1").arg(c + 1) : m_header.value(c));
    preview->setHorizontalHeaderLabels(names);
    const QColor muted = palette().color(QPalette::PlaceholderText);
    for (int r = 0; r < int(m_sample.size()); ++r) {
        const auto &fields = m_sample[std::size_t(r)].fields;
        for (int c = 0; c < int(fields.size()); ++c) {
            const auto &value = fields[std::size_t(c)];
            auto *item = new QTableWidgetItem(value ? shortened(*value) : QStringLiteral("NULL"));
            if (!value)
                item->setForeground(muted);
            preview->setItem(r, c, item);
        }
    }
    fillColumns();
}

void ImportDialog::fillColumns()
{
    const bool create = m_ui->newTable->isChecked();
    QTableWidget *columns = m_ui->columns;
    const QSignalBlocker blocker(columns);
    columns->setRowCount(0);
    const int count = fileColumnCount();
    columns->setRowCount(count);
    std::set<QString> taken;
    for (int c = 0; c < count; ++c) {
        std::vector<std::optional<QString>> values;
        for (const CsvRecord &record : m_sample)
            values.push_back(std::size_t(c) < record.fields.size() ? record.fields[std::size_t(c)]
                                                                   : std::nullopt);
        const QString name = m_header.value(c);
        auto *use = new QTableWidgetItem(name.isEmpty() ? tr("Column %1").arg(c + 1) : name);
        use->setFlags(Qt::ItemIsEnabled | Qt::ItemIsUserCheckable);
        columns->setItem(c, FileColumn, use);
        QString first;
        for (const auto &value : values) {
            if (value) {
                first = shortened(*value);
                break;
            }
        }
        auto *example = new QTableWidgetItem(first);
        example->setFlags(Qt::ItemIsEnabled);
        columns->setItem(c, FirstValue, example);

        if (create) {
            const QString columnName
                = name.isEmpty() ? QStringLiteral("column_%1").arg(c + 1) : loose(name);
            columns->setItem(c, TableColumn, new QTableWidgetItem(columnName));
            auto *type = new QComboBox(columns);
            type->setEditable(true);
            type->addItems(Types);
            type->setCurrentText(catalog::guessType(values));
            connect(type, &QComboBox::currentTextChanged, this, &ImportDialog::updateEnabled);
            columns->setCellWidget(c, TypeColumn, type);
            use->setCheckState(Qt::Checked);
            continue;
        }

        // Into an existing table: by name when the file has a header, by
        // position when it does not; never two file columns into one.
        auto *target = new QComboBox(columns);
        target->addItem(QString());
        target->addItems(m_tableColumns);
        int match = -1;
        for (int t = 0; t < m_tableColumns.size(); ++t) {
            const bool fits = m_header.isEmpty() ? t == c : loose(m_tableColumns[t]) == loose(name);
            if (fits && !taken.contains(m_tableColumns[t])) {
                match = t;
                break;
            }
        }
        if (match >= 0) {
            taken.insert(m_tableColumns[match]);
            target->setCurrentIndex(match + 1);
        }
        use->setCheckState(match >= 0 ? Qt::Checked : Qt::Unchecked);
        connect(target, &QComboBox::currentIndexChanged, this, [this, c, target] {
            if (QTableWidgetItem *item = m_ui->columns->item(c, FileColumn))
                item->setCheckState(target->currentIndex() > 0 ? Qt::Checked : Qt::Unchecked);
            updateType(c);
            updateEnabled();
        });
        columns->setCellWidget(c, TableColumn, target);
        auto *type = new QTableWidgetItem;
        type->setFlags(Qt::ItemIsEnabled);
        columns->setItem(c, TypeColumn, type);
        updateType(c);
    }
    updateEnabled();
}

void ImportDialog::updateType(int row)
{
    auto *target = qobject_cast<QComboBox *>(m_ui->columns->cellWidget(row, TableColumn));
    QTableWidgetItem *type = m_ui->columns->item(row, TypeColumn);
    if (target && type)
        type->setText(m_tableTypes.value(target->currentIndex() - 1));
}

void ImportDialog::updateEnabled()
{
    const bool running = isRunning();
    const bool create = m_ui->newTable->isChecked();
    m_ui->truncate->setEnabled(!create && !running);
    for (QWidget *w :
         {static_cast<QWidget *>(m_ui->formatBox), static_cast<QWidget *>(m_ui->targetBox),
          static_cast<QWidget *>(m_ui->columns), static_cast<QWidget *>(m_ui->path),
          static_cast<QWidget *>(m_ui->browse)}) {
        w->setEnabled(!running);
    }
    m_ui->buttons->button(QDialogButtonBox::Close)->setText(running ? tr("&Stop") : tr("&Close"));

    const catalog::ImportPlan chosen = plan();
    QString problem;
    std::set<QString> names;
    QString twice;
    bool untyped = false;
    for (const catalog::ImportColumn &column : chosen.columns) {
        if (!names.insert(create ? loose(column.name) : column.name).second && twice.isEmpty())
            twice = column.name;
        untyped = untyped || (create && column.type.isEmpty());
    }
    if (chosen.path.isEmpty())
        problem = tr("Choose the file to import.");
    else if (!m_sampleError.isEmpty())
        problem = m_sampleError;
    else if (chosen.table.isEmpty())
        problem = tr("Name the table to import into.");
    else if (!create && m_tableColumns.isEmpty())
        problem = tr("The table's columns are not known yet.");
    else if (chosen.columns.empty())
        problem = tr("Choose at least one column to import.");
    else if (!twice.isEmpty())
        problem = tr("Two file columns go into %1.").arg(twice);
    else if (untyped)
        problem = tr("Give every new column a type.");

    m_ui->buttons->button(QDialogButtonBox::Ok)->setEnabled(problem.isEmpty() && !running);
    if (!running)
        setStatus(problem);
}

void ImportDialog::loadTableColumns()
{
    if (m_ui->newTable->isChecked() || !m_session)
        return;
    const QString schema = m_ui->schema->text().trimmed();
    const QString table = m_ui->table->text().trimmed();
    const QString key = schema + QLatin1Char('.') + table;
    if (table.isEmpty() || key == m_loadedTable)
        return;
    const catalog::SnapshotPtr snapshot = m_session->snapshot();
    const catalog::SchemaObject *object = snapshot ? snapshot->find(schema, table) : nullptr;
    if (!object || object->kind != catalog::ObjectKind::Table) {
        setTableColumns({}, {});
        setStatus(tr("There is no table %1.").arg(key), true);
        return;
    }
    m_loadedTable = key;
    // Generated columns compute their own values.
    QStringList names, types;
    for (const catalog::ColumnInfo &c : object->columns) {
        if (c.isGenerated() || c.hidden == 1)
            continue;
        names << c.name;
        types << c.type;
    }
    setTableColumns(names, types);
}

void ImportDialog::startImport()
{
    if (isRunning() || !m_session || !m_session->runner()
        || !m_ui->buttons->button(QDialogButtonBox::Ok)->isEnabled())
        return;
    catalog::ImportPlan chosen = plan();
    // What a field turns into goes by the column's declared type.
    if (!chosen.create) {
        for (catalog::ImportColumn &column : chosen.columns)
            column.type = m_tableTypes.value(m_tableColumns.indexOf(column.name));
    }
    m_job = new catalog::ImportJob(m_session->runner(), chosen, this);
    connect(m_job, &catalog::ImportJob::progress, this,
            [this](qint64 bytes, qint64 total, qint64 rows) {
                m_ui->progress->setValue(total > 0 ? int(bytes * 1000 / total) : 0);
                setStatus(tr("%1 rows imported…").arg(rows));
            });
    connect(m_job, &catalog::ImportJob::finished, this, &ImportDialog::finish);
    m_ui->progress->setValue(0);
    m_ui->progress->show();
    updateEnabled();
    setStatus(tr("Importing…"));
    m_job->start();
}

void ImportDialog::finish(bool ok, const QString &message)
{
    const catalog::ImportPlan chosen = plan();
    if (m_job) {
        m_job->disconnect(this);
        m_job->deleteLater();
        m_job = nullptr;
    }
    m_ui->progress->setValue(ok ? m_ui->progress->maximum() : 0);
    if (ok) {
        // The table is there now: importing again goes into it.
        m_loadedTable.clear();
        m_ui->newTable->setChecked(false);
        m_ui->existingTable->setChecked(true);
        Q_EMIT imported(chosen.schema, chosen.table);
    }
    updateEnabled();
    setStatus(message, !ok);
}

void ImportDialog::setStatus(const QString &text, bool error)
{
    m_ui->status->setText(text);
    // A red that reads on a dark background too.
    const bool dark = palette().color(QPalette::Window).lightness() < 128;
    m_ui->status->setStyleSheet(
        error ? QStringLiteral("color: %1;")
                    .arg(dark ? QStringLiteral("#ff7b72") : QStringLiteral("#c62828"))
              : QString());
}

void ImportDialog::reject()
{
    if (!isRunning()) {
        QDialog::reject();
        return;
    }
    m_job->cancel();
}

} // namespace sqliteman
