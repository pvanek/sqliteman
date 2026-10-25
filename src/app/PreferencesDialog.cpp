// SPDX-FileCopyrightText: 2007-2026 Petr Vanek
// SPDX-License-Identifier: GPL-3.0-or-later

#include "PreferencesDialog.h"

#include "ui_PreferencesDialog.h"

#include <QColorDialog>
#include <QFileDialog>
#include <QFontDatabase>
#include <QFontDialog>
#include <QHeaderView>
#include <QPushButton>

namespace sqliteman {

PreferencesDialog::PreferencesDialog(const config::Preferences &preferences, QWidget *parent)
    : QDialog(parent), m_ui(std::make_unique<Ui::PreferencesDialog>())
{
    m_ui->setupUi(this);
    m_ui->snippets->setColumnCount(2);
    m_ui->snippets->setHorizontalHeaderLabels({tr("Word"), tr("Text")});
    m_ui->snippets->horizontalHeader()->setStretchLastSection(true);

    auto colorButton = [this](QPushButton *button, QColor *color) {
        connect(button, &QPushButton::clicked, this, [this, button, color] {
            const QColor chosen = QColorDialog::getColor(
                color->isValid() ? *color : palette().color(QPalette::Base), this);
            if (chosen.isValid()) {
                *color = chosen;
                showColor(button, chosen);
            }
        });
        button->setContextMenuPolicy(Qt::CustomContextMenu);
        connect(button, &QWidget::customContextMenuRequested, this, [this, button, color] {
            *color = QColor();
            showColor(button, *color);
        });
    };
    colorButton(m_ui->nullColor, &m_nullColor);
    colorButton(m_ui->blobColor, &m_blobColor);

    connect(m_ui->font, &QPushButton::clicked, this, [this] {
        QFont current;
        if (m_font.isEmpty() || !current.fromString(m_font))
            current = QFontDatabase::systemFont(QFontDatabase::FixedFont);
        bool ok = false;
        const QFont chosen = QFontDialog::getFont(&ok, current, this, tr("Editor Font"),
                                                  QFontDialog::MonospacedFonts);
        if (ok) {
            m_font = chosen.toString();
            showFont();
        }
    });
    m_ui->font->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_ui->font, &QWidget::customContextMenuRequested, this, [this] {
        m_font.clear();
        showFont();
    });
    connect(m_ui->completion, &QCheckBox::toggled, m_ui->completionChars, &QWidget::setEnabled);
    connect(m_ui->useSnippets, &QCheckBox::toggled, m_ui->snippets, &QWidget::setEnabled);
    connect(m_ui->addSnippet, &QPushButton::clicked, this, [this] {
        addSnippet(QString(), QString());
        const int row = m_ui->snippets->rowCount() - 1;
        m_ui->snippets->editItem(m_ui->snippets->item(row, 0));
    });
    connect(m_ui->removeSnippet, &QPushButton::clicked, this, [this] {
        if (m_ui->snippets->currentRow() >= 0)
            m_ui->snippets->removeRow(m_ui->snippets->currentRow());
    });
    connect(m_ui->allowExtensions, &QCheckBox::toggled, this, [this](bool on) {
        for (QWidget *w :
             {static_cast<QWidget *>(m_ui->extensions), static_cast<QWidget *>(m_ui->addExtension),
              static_cast<QWidget *>(m_ui->removeExtension)})
            w->setEnabled(on);
    });
    connect(m_ui->addExtension, &QPushButton::clicked, this, [this] {
        const QStringList paths = QFileDialog::getOpenFileNames(this, tr("Add Extension"));
        for (const QString &path : paths)
            m_ui->extensions->addItem(path);
    });
    connect(m_ui->removeExtension, &QPushButton::clicked, this,
            [this] { qDeleteAll(m_ui->extensions->selectedItems()); });
    connect(m_ui->buttons->button(QDialogButtonBox::RestoreDefaults), &QPushButton::clicked, this,
            [this] {
                config::Preferences defaults;
                defaults.snippets = config::Preferences::defaultSnippets();
                setPreferences(defaults);
            });
    connect(m_ui->buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(m_ui->buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    setPreferences(preferences);
}

PreferencesDialog::~PreferencesDialog() = default;

void PreferencesDialog::showColor(QPushButton *button, const QColor &color)
{
    button->setText(color.isValid() ? color.name() : tr("Default"));
    button->setStyleSheet(
        color.isValid() ? QStringLiteral("background-color: %1;").arg(color.name()) : QString());
}

void PreferencesDialog::showFont()
{
    QFont font;
    if (m_font.isEmpty() || !font.fromString(m_font)) {
        m_ui->font->setText(tr("The system's fixed font"));
        m_ui->font->setFont(QFont());
        return;
    }
    m_ui->font->setText(QStringLiteral("%1 %2").arg(font.family()).arg(font.pointSize()));
}

void PreferencesDialog::addSnippet(const QString &key, const QString &text)
{
    const int row = m_ui->snippets->rowCount();
    m_ui->snippets->insertRow(row);
    m_ui->snippets->setItem(row, 0, new QTableWidgetItem(key));
    // The text on one line: \n stands for a line break.
    QString shown = text;
    shown.replace(QLatin1Char('\n'), QLatin1String("\\n"));
    m_ui->snippets->setItem(row, 1, new QTableWidgetItem(shown));
}

void PreferencesDialog::setPreferences(const config::Preferences &p)
{
    m_ui->recentCount->setValue(p.recentCount);
    m_ui->reopenDatabases->setChecked(p.reopenDatabases);

    m_ui->nullText->setText(p.nullText);
    m_ui->blobText->setText(p.blobText);
    m_ui->highlightNull->setChecked(p.highlightNull);
    m_ui->highlightBlob->setChecked(p.highlightBlob);
    m_nullColor = p.nullColor;
    m_blobColor = p.blobColor;
    showColor(m_ui->nullColor, m_nullColor);
    showColor(m_ui->blobColor, m_blobColor);
    m_ui->rowLimit->setValue(p.rowLimit);

    m_font = p.editorFont;
    showFont();
    m_ui->highlightCurrentLine->setChecked(p.highlightCurrentLine);
    m_ui->widthMark->setValue(p.widthMark);
    m_ui->completion->setChecked(p.completion);
    m_ui->completionChars->setValue(p.completionChars);
    m_ui->completionChars->setEnabled(p.completion);
    m_ui->useSnippets->setChecked(p.useSnippets);
    m_ui->snippets->setEnabled(p.useSnippets);
    m_ui->snippets->setRowCount(0);
    for (auto it = p.snippets.cbegin(); it != p.snippets.cend(); ++it)
        addSnippet(it.key(), it.value());

    m_ui->foreignKeys->setChecked(p.foreignKeys);
    m_ui->busyTimeout->setValue(p.busyTimeoutMs);
    m_ui->allowExtensions->setChecked(p.allowExtensions);
    m_ui->extensions->clear();
    m_ui->extensions->addItems(p.startupExtensions);
    for (QWidget *w :
         {static_cast<QWidget *>(m_ui->extensions), static_cast<QWidget *>(m_ui->addExtension),
          static_cast<QWidget *>(m_ui->removeExtension)})
        w->setEnabled(p.allowExtensions);
}

config::Preferences PreferencesDialog::preferences() const
{
    config::Preferences p;
    p.recentCount = m_ui->recentCount->value();
    p.reopenDatabases = m_ui->reopenDatabases->isChecked();

    p.nullText = m_ui->nullText->text();
    p.blobText = m_ui->blobText->text();
    p.highlightNull = m_ui->highlightNull->isChecked();
    p.highlightBlob = m_ui->highlightBlob->isChecked();
    p.nullColor = m_nullColor;
    p.blobColor = m_blobColor;
    p.rowLimit = m_ui->rowLimit->value();

    p.editorFont = m_font;
    p.highlightCurrentLine = m_ui->highlightCurrentLine->isChecked();
    p.widthMark = m_ui->widthMark->value();
    p.completion = m_ui->completion->isChecked();
    p.completionChars = m_ui->completionChars->value();
    p.useSnippets = m_ui->useSnippets->isChecked();
    for (int row = 0; row < m_ui->snippets->rowCount(); ++row) {
        const QTableWidgetItem *key = m_ui->snippets->item(row, 0);
        const QTableWidgetItem *text = m_ui->snippets->item(row, 1);
        if (!key || key->text().trimmed().isEmpty())
            continue;
        QString value = text ? text->text() : QString();
        value.replace(QLatin1String("\\n"), QLatin1String("\n"));
        p.snippets.insert(key->text().trimmed(), value);
    }

    p.foreignKeys = m_ui->foreignKeys->isChecked();
    p.busyTimeoutMs = m_ui->busyTimeout->value();
    p.allowExtensions = m_ui->allowExtensions->isChecked();
    for (int i = 0; i < m_ui->extensions->count(); ++i)
        p.startupExtensions << m_ui->extensions->item(i)->text();
    return p;
}

} // namespace sqliteman
