// SPDX-FileCopyrightText: 2026 Petr Vanek
// SPDX-License-Identifier: GPL-3.0-or-later

#include "ValueViewer.h"

#include "ResultTextView.h"
#include "Settings.h"
#include "SqlEditor.h"
#include "ValueLexer.h"

#include "ui_ValueViewer.h"

#include <QClipboard>
#include <QEvent>
#include <QFile>
#include <QFileDialog>
#include <QGuiApplication>
#include <QLocale>
#include <QMessageBox>
#include <QPixmap>
#include <QPushButton>
#include <QStringDecoder>

namespace sqliteman {

using sqlite::ValueType;

namespace {

const QString FormatKey = QStringLiteral("valueViewer/format");
const QString WrapKey = QStringLiteral("valueViewer/wrap");
const QString DirectoryKey = QStringLiteral("valueViewer/directory");

// So many bytes of a blob are shown in hex; Save writes all of it.
constexpr qsizetype MaxHexBytes = 1 << 20;

// Offset, sixteen bytes in hex, and those that are printable as they are.
QString hexDump(const QByteArray &bytes)
{
    QString out;
    const qsizetype shown = std::min(bytes.size(), MaxHexBytes);
    out.reserve(shown * 4 + shown / 16 * 12);
    for (qsizetype line = 0; line < shown; line += 16) {
        out += QStringLiteral("%1  ").arg(line, 8, 16, QLatin1Char('0'));
        QString printable;
        for (qsizetype i = line; i < line + 16; ++i) {
            if (i < shown) {
                const auto byte = uchar(bytes[i]);
                out += QStringLiteral("%1 ").arg(byte, 2, 16, QLatin1Char('0'));
                printable
                    += byte >= 0x20 && byte < 0x7f ? QLatin1Char(char(byte)) : QLatin1Char('.');
            } else {
                out += QLatin1String("   ");
            }
            if (i == line + 7)
                out += QLatin1Char(' ');
        }
        out += QLatin1Char(' ') + printable + QLatin1Char('\n');
    }
    if (shown < bytes.size())
        out += QStringLiteral("…\n");
    return out;
}

bool isValidUtf8(const QByteArray &bytes)
{
    QStringDecoder decoder(QStringDecoder::Utf8, QStringDecoder::Flag::Stateless);
    const QString text = decoder.decode(bytes);
    return !decoder.hasError() && !text.contains(QChar(0));
}

} // namespace

ValueViewer::ValueViewer(const QString &title, const sqlite::Value &value, bool editable,
                         QWidget *parent)
    : QDialog(parent), m_ui(std::make_unique<Ui::ValueViewer>()), m_original(value), m_value(value),
      m_editable(editable)
{
    m_ui->setupUi(this);
    setWindowTitle(title);
    // Not the block selection the grid's text view explains in its tooltip:
    // here it is mostly lines that get copied.
    m_ui->text->setToolTip(QString());
    m_ui->text->setMarginType(0, QsciScintilla::NumberMargin);
    m_ui->text->setFont(Settings::instance().editorFont());
    m_ui->hex->setToolTip(QString());
    m_ui->hex->setFont(Settings::instance().editorFont());
    m_lexer = new ValueLexer(this);
    m_ui->text->setLexer(m_lexer);
    SqlEditor::applyMarginColors(m_ui->text);

    const QSettings &settings = Settings::instance().store();
    m_ui->format->setChecked(settings.value(FormatKey, true).toBool());
    setWrapLines(settings.value(WrapKey, false).toBool());
    m_ui->syntax->setCurrentIndex(
        value.type() == ValueType::Blob ? 0 : int(catalog::guessSyntax(value.data())));

    m_ui->null->setChecked(value.isNull());
    m_ui->null->setVisible(editable);
    m_ui->load->setVisible(editable);
    if (editable)
        m_ui->buttons->setStandardButtons(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);

    connect(m_ui->syntax, &QComboBox::currentIndexChanged, this, &ValueViewer::showValue);
    connect(m_ui->format, &QCheckBox::toggled, this, [this](bool on) {
        Settings::instance().store().setValue(FormatKey, on);
        showValue();
    });
    connect(m_ui->wrap, &QCheckBox::toggled, this, [this](bool on) {
        Settings::instance().store().setValue(WrapKey, on);
        setWrapLines(on);
    });
    connect(m_ui->null, &QCheckBox::toggled, this, &ValueViewer::updateNull);
    connect(m_ui->copy, &QPushButton::clicked, this,
            [this] { QGuiApplication::clipboard()->setText(text()); });
    connect(m_ui->load, &QPushButton::clicked, this, [this] {
        QSettings &store = Settings::instance().store();
        const QString path = QFileDialog::getOpenFileName(this, tr("Load File"),
                                                          store.value(DirectoryKey).toString());
        if (path.isEmpty())
            return;
        store.setValue(DirectoryKey, QFileInfo(path).absolutePath());
        if (!loadFile(path))
            QMessageBox::warning(this, tr("Load File"), tr("Could not read %1.").arg(path));
    });
    connect(m_ui->save, &QPushButton::clicked, this, [this] {
        QSettings &store = Settings::instance().store();
        const QString path = QFileDialog::getSaveFileName(this, tr("Save to File"),
                                                          store.value(DirectoryKey).toString());
        if (path.isEmpty())
            return;
        store.setValue(DirectoryKey, QFileInfo(path).absolutePath());
        if (!saveFile(path))
            QMessageBox::warning(this, tr("Save to File"), tr("Could not write %1.").arg(path));
    });
    connect(m_ui->buttons, &QDialogButtonBox::accepted, this, [this] {
        Q_EMIT valueAccepted(this->value(), isTypedText());
        accept();
    });
    connect(m_ui->buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(m_ui->text, &QsciScintilla::textChanged, this, [this] {
        if (!m_ui->text->isReadOnly())
            m_textEdited = true;
    });
    showValue();
    updateNull();
    m_ui->text->setFocus();
}

ValueViewer::~ValueViewer() = default;

sqlite::Value ValueViewer::value() const
{
    if (m_ui->null->isChecked())
        return {};
    if (m_textEdited)
        return sqlite::Value::text(m_ui->text->text());
    return m_value;
}

bool ValueViewer::isTypedText() const
{
    return m_textEdited && !m_ui->null->isChecked();
}

bool ValueViewer::isModified() const
{
    return !(value() == m_original);
}

bool ValueViewer::loadFile(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return false;
    m_value = sqlite::Value::blob(file.readAll());
    m_textEdited = false;
    m_ui->null->setChecked(false);
    m_ui->syntax->setCurrentIndex(0);
    showValue();
    return true;
}

bool ValueViewer::saveFile(const QString &path) const
{
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly))
        return false;
    const sqlite::Value v = value();
    const QByteArray bytes = v.type() == ValueType::Text ? v.toString().toUtf8() : v.data();
    return file.write(bytes) == bytes.size();
}

catalog::ValueSyntax ValueViewer::syntax() const
{
    return catalog::ValueSyntax(m_ui->syntax->currentIndex());
}

void ValueViewer::setSyntax(catalog::ValueSyntax syntax)
{
    m_ui->syntax->setCurrentIndex(int(syntax));
}

bool ValueViewer::isFormatted() const
{
    return m_ui->format->isChecked();
}

void ValueViewer::setFormatted(bool formatted)
{
    m_ui->format->setChecked(formatted);
}

bool ValueViewer::wrapsLines() const
{
    return m_ui->wrap->isChecked();
}

void ValueViewer::setWrapLines(bool wrap)
{
    m_ui->wrap->setChecked(wrap);
    m_ui->text->setWrapMode(wrap ? QsciScintilla::WrapWord : QsciScintilla::WrapNone);
}

QString ValueViewer::text() const
{
    return m_ui->text->text();
}

QString ValueViewer::status() const
{
    return m_ui->status->text();
}

ResultTextView *ValueViewer::editor() const
{
    return m_ui->text;
}

bool ValueViewer::showsImage() const
{
    return m_ui->pages->isTabVisible(m_ui->pages->indexOf(m_ui->imagePage));
}

void ValueViewer::updateNull()
{
    const bool null = m_ui->null->isChecked();
    m_ui->pages->setEnabled(!null);
    m_ui->save->setEnabled(!null);
    m_ui->type->setText(null ? QStringLiteral("null") : sqlite::typeName(m_value.type()));
}

void ValueViewer::showValue()
{
    const bool blob = m_value.type() == ValueType::Blob;
    const QByteArray &bytes = m_value.data();
    const catalog::ValueSyntax syntax = this->syntax();
    m_ui->format->setEnabled(syntax != catalog::ValueSyntax::Text && (!blob || isValidUtf8(bytes)));

    // A blob that is not text cannot be edited, or read, as such.
    const bool readableText = !blob || isValidUtf8(bytes);
    m_ui->syntax->setEnabled(readableText);
    m_ui->wrap->setEnabled(readableText);
    QByteArray shown = readableText ? bytes : QByteArray();
    QString problem;
    if (syntax != catalog::ValueSyntax::Text && isFormatted()) {
        if (std::optional<QByteArray> pretty = catalog::pretty(syntax, bytes))
            shown = std::move(*pretty);
        else
            problem = tr("Not valid JSON, so shown as it is.");
    }
    // Tokens and text from the same bytes: Scintilla's positions are bytes
    // of the UTF-8 it holds.
    m_ui->text->setReadOnly(true);
    m_lexer->setTokens(catalog::tokenize(syntax, shown));
    m_ui->text->setText(QString::fromUtf8(shown));
    m_ui->text->recolor();
    m_ui->text->setReadOnly(!m_editable || !readableText);
    m_textEdited = false;

    const int lines = m_ui->text->lines();
    m_ui->text->setMarginWidth(0, QString(QString::number(lines).size() + 1, QLatin1Char('9')));

    QPixmap picture;
    const bool image = blob && picture.loadFromData(bytes);
    m_ui->pages->setTabVisible(m_ui->pages->indexOf(m_ui->imagePage), image);
    if (image)
        m_ui->image->setPixmap(picture);
    m_ui->pages->setTabVisible(m_ui->pages->indexOf(m_ui->hexPage), blob);
    if (blob)
        m_ui->hex->setText(hexDump(bytes));
    m_ui->pages->setTabVisible(m_ui->pages->indexOf(m_ui->textPage), readableText);
    if (image)
        m_ui->pages->setCurrentWidget(m_ui->imagePage);
    else if (!readableText)
        m_ui->pages->setCurrentWidget(m_ui->hexPage);
    else
        m_ui->pages->setCurrentWidget(m_ui->textPage);

    QString size;
    if (blob) {
        size = QLocale().formattedDataSize(bytes.size(), 1, QLocale::DataSizeTraditionalFormat);
        if (image)
            size += QStringLiteral(", %1 × %2").arg(picture.width()).arg(picture.height());
    } else {
        size = tr("%1 characters, %n line(s)", nullptr, lines)
                   .arg(QLocale().toString(QString::fromUtf8(bytes).size()));
    }
    m_ui->status->setText(problem.isEmpty() ? size : problem);
    m_ui->status->setToolTip(problem.isEmpty() ? QString() : size);
    updateNull();
}

void ValueViewer::changeEvent(QEvent *event)
{
    QDialog::changeEvent(event);
    if (event->type() == QEvent::PaletteChange
        || event->type() == QEvent::ApplicationPaletteChange) {
        m_lexer->applyPalette();
        SqlEditor::applyMarginColors(m_ui->text);
    }
}

} // namespace sqliteman
