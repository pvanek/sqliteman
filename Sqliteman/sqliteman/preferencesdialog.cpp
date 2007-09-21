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


PreferencesDialog::PreferencesDialog(QWidget * parent)
	: QDialog(parent)
{
	ui.setupUi(this);

	connect(ui.nullBgButton, SIGNAL(clicked()), this, SLOT(nullBgButton_clicked()));
	connect(ui.blobBgButton, SIGNAL(clicked()), this, SLOT(blobBgButton_clicked()));
	connect(ui.activeHighlightButton, SIGNAL(clicked()), this, SLOT(activeHighlightButton_clicked()));
	connect(ui.buttonBox->button(QDialogButtonBox::RestoreDefaults), SIGNAL(clicked()), this, SLOT(restoreDefaults()));
	connect(ui.shortcutsButton, SIGNAL(clicked()), this, SLOT(shortcutsButton_clicked()));

	Preferences * prefs = Preferences::instance();

	// avail langs
	QDir d(TRANSLATION_DIR, "*.qm");
	ui.languageComboBox->addItem(tr("From Locales"));
	foreach (QString f, d.entryList())
		ui.languageComboBox->addItem(f.remove("sqliteman_").remove(".qm"));
	ui.languageComboBox->setCurrentIndex(prefs->GUItranslator());

	// avail styles
	ui.styleComboBox->addItem(tr("System Predefined"));
	QStringList sl(QStyleFactory::keys());
	sl.sort();
	ui.styleComboBox->addItems(sl);
	ui.styleComboBox->setCurrentIndex(prefs->GUIstyle());
	ui.recentlyUsedSpinBox->setValue(prefs->recentlyUsedCount());

	ui.nullCheckBox->setChecked(prefs->nullHighlight());
	ui.nullAliasEdit->setText(prefs->nullHighlightText());
	ui.nullBgButton->setPalette(prefs->nullHighlightColor());

	ui.blobCheckBox->setChecked(prefs->blobHighlight());
	ui.blobAliasEdit->setText(prefs->blobHighlightText());
	ui.blobBgButton->setPalette(prefs->blobHighlightColor());

	ui.cropColumnsCheckBox->setChecked(prefs->cropColumns());

	ui.fontComboBox->setCurrentFont(prefs->sqlFont());
    ui.fontSizeSpin->setValue(prefs->sqlFontSize());
	ui.useActiveHighlightCheckBox->setChecked(prefs->activeHighlighting());
	ui.activeHighlightButton->setPalette(prefs->activeHighlightColor());
	ui.useTextWidthMarkCheckBox->setChecked(prefs->textWidthMark());
	ui.textWidthMarkSpinBox->setValue(prefs->textWidthMarkSize());
	ui.useCompletionCheck->setChecked(prefs->codeCompletion());
	ui.completionLengthBox->setValue(prefs->codeCompletionLength());
	ui.useShortcutsBox->setChecked(prefs->useShortcuts());
}

bool PreferencesDialog::saveSettings()
{
	Preferences * prefs = Preferences::instance();
	prefs->setGUItranslator(ui.languageComboBox->currentIndex());
	prefs->setGUIstyle(ui.styleComboBox->currentIndex());
	prefs->setRecentlyUsedCount(ui.recentlyUsedSpinBox->value());
	// data results
	prefs->setNullHighlight(ui.nullCheckBox->isChecked());
	prefs->setNullHighlightText(ui.nullAliasEdit->text());
	prefs->setNullHighlightColor(ui.nullBgButton->palette().color(QPalette::Background));
	prefs->setBlobHighlight(ui.blobCheckBox->isChecked());
	prefs->setBlobHighlightText(ui.blobAliasEdit->text());
	prefs->setBlobHighlightColor(ui.blobBgButton->palette().color(QPalette::Background));
	prefs->setCropColumns(ui.cropColumnsCheckBox->isChecked());
	// sql editor
	prefs->setSqlFont(ui.fontComboBox->currentFont());
    prefs->setSqlFontSize(ui.fontSizeSpin->value());
	prefs->setActiveHighlighting(ui.useActiveHighlightCheckBox->isChecked());
	prefs->setActiveHighlightColor(ui.activeHighlightButton->palette().color(QPalette::Background));
	prefs->setTextWidthMark(ui.useTextWidthMarkCheckBox->isChecked());
	prefs->setTextWidthMarkSize(ui.textWidthMarkSpinBox->value());
	prefs->setCodeCompletion(ui.useCompletionCheck->isChecked());
	prefs->setCodeCompletionLength(ui.completionLengthBox->value());
	prefs->setUseShortcuts(ui.useShortcutsBox->isChecked());

	return true;
}

void PreferencesDialog::restoreDefaults()
{
	QColor defCol(255, 254, 205);

	ui.languageComboBox->setCurrentIndex(0);
	ui.styleComboBox->setCurrentIndex(0);

	ui.nullCheckBox->setChecked(true);
	ui.nullAliasEdit->setText("{null}");
	ui.nullBgButton->setPalette(defCol);

	ui.blobCheckBox->setChecked(true);
	ui.blobAliasEdit->setText("{blob}");
	ui.blobBgButton->setPalette(defCol);

	ui.cropColumnsCheckBox->setChecked(false);

    QFont fTmp;
	ui.fontComboBox->setCurrentFont(fTmp);
    ui.fontSizeSpin->setValue(fTmp.pointSize());
	ui.useActiveHighlightCheckBox->setChecked(true);
	ui.activeHighlightButton->setPalette(defCol);
	ui.useTextWidthMarkCheckBox->setChecked(true);
	ui.textWidthMarkSpinBox->setValue(75);
	ui.useCompletionCheck->setChecked(false);
	ui.completionLengthBox->setValue(3);
	ui.useShortcutsBox->setChecked(false);
}

void PreferencesDialog::blobBgButton_clicked()
{
	QColor nCol = QColorDialog::getColor(ui.blobBgButton->palette().color(QPalette::Background), this);
	if (nCol.isValid())
		ui.blobBgButton->setPalette(nCol);
}

void PreferencesDialog::nullBgButton_clicked()
{
	QColor nCol = QColorDialog::getColor(ui.nullBgButton->palette().color(QPalette::Background), this);
	if (nCol.isValid())
		ui.nullBgButton->setPalette(nCol);
}

void PreferencesDialog::activeHighlightButton_clicked()
{
	QColor nCol = QColorDialog::getColor(ui.activeHighlightButton->palette().color(QPalette::Background), this);
	if (nCol.isValid())
		ui.activeHighlightButton->setPalette(nCol);
}

void PreferencesDialog::shortcutsButton_clicked()
{
	ShortcutEditorDialog d;
	d.exec();
}
