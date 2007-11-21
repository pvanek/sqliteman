/*
For general Sqliteman copyright and licensing information please refer
to the COPYING file provided with the program. Following this notice may exist
a copyright and/or license notice that predates the release of Sqliteman
for which a new license (GPL+exception) is in place.
*/

#include <QDir>
#include <QStyleFactory>
#include <QSettings>
#include <QColorDialog>

#include "preferencesdialog.h"
#include "preferences.h"
#include "shortcuteditordialog.h"
#include "utils.h"


PrefsDataDisplayWidget::PrefsDataDisplayWidget(QWidget * parent)
	: QDialog(parent)
{
	setupUi(this);
}

PrefsLNFWidget::PrefsLNFWidget(QWidget * parent)
	: QDialog(parent)
{
	setupUi(this);
}

PrefsSQLEditorWidget::PrefsSQLEditorWidget(QWidget * parent)
	: QDialog(parent)
{
	setupUi(this);
}


PreferencesDialog::PreferencesDialog(QWidget * parent)
	: QDialog(parent)
{
	setupUi(this);

	m_prefsData = new PrefsDataDisplayWidget(this);
	m_prefsLNF = new PrefsLNFWidget(this);
	m_prefsSQL = new PrefsSQLEditorWidget(this);

	stackedWidget->addWidget(m_prefsLNF);
	stackedWidget->addWidget(m_prefsData);
	stackedWidget->addWidget(m_prefsSQL);
	stackedWidget->setCurrentIndex(0);

	listWidget->addItem(new QListWidgetItem(getIcon("preferences-desktop-display.png"), m_prefsLNF->titleLabel->text(), listWidget));
	listWidget->addItem(new QListWidgetItem(getIcon("table.png"), m_prefsData->titleLabel->text(), listWidget));
	listWidget->addItem(new QListWidgetItem(getIcon("kate.png"), m_prefsSQL->titleLabel->text(), listWidget));
	listWidget->setCurrentRow(0);

	connect(m_prefsData->nullBgButton, SIGNAL(clicked()),
			this, SLOT(nullBgButton_clicked()));
	connect(m_prefsData->blobBgButton, SIGNAL(clicked()),
			this, SLOT(blobBgButton_clicked()));
	connect(m_prefsSQL->activeHighlightButton, SIGNAL(clicked()),
			this, SLOT(activeHighlightButton_clicked()));
	connect(buttonBox->button(QDialogButtonBox::RestoreDefaults),
			SIGNAL(clicked()), this, SLOT(restoreDefaults()));
	connect(m_prefsSQL->shortcutsButton, SIGNAL(clicked()),
			this, SLOT(shortcutsButton_clicked()));
	// change prefs widgets
	connect(listWidget, SIGNAL(currentRowChanged(int)),
			stackedWidget, SLOT(setCurrentIndex(int)));

	Preferences * prefs = Preferences::instance();

	// avail langs
	QDir d(TRANSLATION_DIR, "*.qm");
	m_prefsLNF->languageComboBox->addItem(tr("From Locales"));
	foreach (QString f, d.entryList())
		m_prefsLNF->languageComboBox->addItem(f.remove("sqliteman_").remove(".qm"));
	m_prefsLNF->languageComboBox->setCurrentIndex(prefs->GUItranslator());

	// avail styles
	m_prefsLNF->styleComboBox->addItem(tr("System Predefined"));
	QStringList sl(QStyleFactory::keys());
	sl.sort();
	m_prefsLNF->styleComboBox->addItems(sl);
	m_prefsLNF->styleComboBox->setCurrentIndex(prefs->GUIstyle());
	m_prefsLNF->recentlyUsedSpinBox->setValue(prefs->recentlyUsedCount());

	m_prefsData->nullCheckBox->setChecked(prefs->nullHighlight());
	m_prefsData->nullAliasEdit->setText(prefs->nullHighlightText());
	m_prefsData->nullBgButton->setPalette(prefs->nullHighlightColor());

	m_prefsData->blobCheckBox->setChecked(prefs->blobHighlight());
	m_prefsData->blobAliasEdit->setText(prefs->blobHighlightText());
	m_prefsData->blobBgButton->setPalette(prefs->blobHighlightColor());

	m_prefsData->cropColumnsCheckBox->setChecked(prefs->cropColumns());

	m_prefsSQL->fontComboBox->setCurrentFont(prefs->sqlFont());
	m_prefsSQL->fontSizeSpin->setValue(prefs->sqlFontSize());
	m_prefsSQL->useActiveHighlightCheckBox->setChecked(prefs->activeHighlighting());
	m_prefsSQL->activeHighlightButton->setPalette(prefs->activeHighlightColor());
	m_prefsSQL->useTextWidthMarkCheckBox->setChecked(prefs->textWidthMark());
	m_prefsSQL->textWidthMarkSpinBox->setValue(prefs->textWidthMarkSize());
	m_prefsSQL->useCompletionCheck->setChecked(prefs->codeCompletion());
	m_prefsSQL->completionLengthBox->setValue(prefs->codeCompletionLength());
	m_prefsSQL->useShortcutsBox->setChecked(prefs->useShortcuts());
}

bool PreferencesDialog::saveSettings()
{
	Preferences * prefs = Preferences::instance();
	prefs->setGUItranslator(m_prefsLNF->languageComboBox->currentIndex());
	prefs->setGUIstyle(m_prefsLNF->styleComboBox->currentIndex());
	prefs->setRecentlyUsedCount(m_prefsLNF->recentlyUsedSpinBox->value());
	// data results
	prefs->setNullHighlight(m_prefsData->nullCheckBox->isChecked());
	prefs->setNullHighlightText(m_prefsData->nullAliasEdit->text());
	prefs->setNullHighlightColor(m_prefsData->nullBgButton->palette().color(QPalette::Background));
	prefs->setBlobHighlight(m_prefsData->blobCheckBox->isChecked());
	prefs->setBlobHighlightText(m_prefsData->blobAliasEdit->text());
	prefs->setBlobHighlightColor(m_prefsData->blobBgButton->palette().color(QPalette::Background));
	prefs->setCropColumns(m_prefsData->cropColumnsCheckBox->isChecked());
	// sql editor
	prefs->setSqlFont(m_prefsSQL->fontComboBox->currentFont());
	prefs->setSqlFontSize(m_prefsSQL->fontSizeSpin->value());
	prefs->setActiveHighlighting(m_prefsSQL->useActiveHighlightCheckBox->isChecked());
	prefs->setActiveHighlightColor(m_prefsSQL->activeHighlightButton->palette().color(QPalette::Background));
	prefs->setTextWidthMark(m_prefsSQL->useTextWidthMarkCheckBox->isChecked());
	prefs->setTextWidthMarkSize(m_prefsSQL->textWidthMarkSpinBox->value());
	prefs->setCodeCompletion(m_prefsSQL->useCompletionCheck->isChecked());
	prefs->setCodeCompletionLength(m_prefsSQL->completionLengthBox->value());
	prefs->setUseShortcuts(m_prefsSQL->useShortcutsBox->isChecked());

	return true;
}

void PreferencesDialog::restoreDefaults()
{
	m_prefsLNF->languageComboBox->setCurrentIndex(0);
	m_prefsLNF->styleComboBox->setCurrentIndex(0);
	m_prefsLNF->recentlyUsedSpinBox->setValue(5);

	m_prefsData->nullCheckBox->setChecked(true);
	m_prefsData->nullAliasEdit->setText("{null}");
	m_prefsData->nullBgButton->setPalette(Preferences::stdLightColor());

	m_prefsData->blobCheckBox->setChecked(true);
	m_prefsData->blobAliasEdit->setText("{blob}");
	m_prefsData->blobBgButton->setPalette(Preferences::stdLightColor());

	m_prefsData->cropColumnsCheckBox->setChecked(false);

	QFont fTmp;
	m_prefsSQL->fontComboBox->setCurrentFont(fTmp);
	m_prefsSQL->fontSizeSpin->setValue(fTmp.pointSize());
	m_prefsSQL->useActiveHighlightCheckBox->setChecked(true);
	m_prefsSQL->activeHighlightButton->setPalette(Preferences::stdDarkColor());
	m_prefsSQL->useTextWidthMarkCheckBox->setChecked(true);
	m_prefsSQL->textWidthMarkSpinBox->setValue(75);
	m_prefsSQL->useCompletionCheck->setChecked(false);
	m_prefsSQL->completionLengthBox->setValue(3);
	m_prefsSQL->useShortcutsBox->setChecked(false);
}

void PreferencesDialog::blobBgButton_clicked()
{
	QColor nCol = QColorDialog::getColor(m_prefsData->blobBgButton->palette().color(QPalette::Background), this);
	if (nCol.isValid())
		m_prefsData->blobBgButton->setPalette(nCol);
}

void PreferencesDialog::nullBgButton_clicked()
{
	QColor nCol = QColorDialog::getColor(m_prefsData->nullBgButton->palette().color(QPalette::Background), this);
	if (nCol.isValid())
		m_prefsData->nullBgButton->setPalette(nCol);
}

void PreferencesDialog::activeHighlightButton_clicked()
{
	QColor nCol = QColorDialog::getColor(m_prefsSQL->activeHighlightButton->palette().color(QPalette::Background), this);
	if (nCol.isValid())
		m_prefsSQL->activeHighlightButton->setPalette(nCol);
}

void PreferencesDialog::shortcutsButton_clicked()
{
	ShortcutEditorDialog d;
	d.exec();
}
