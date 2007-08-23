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

	// avail langs
	QDir d(TRANSLATION_DIR, "*.qm");
	ui.languageComboBox->addItem(tr("From Locales"));
	foreach (QString f, d.entryList())
		ui.languageComboBox->addItem(f.remove("sqliteman_").remove(".qm"));
	ui.languageComboBox->setCurrentIndex(GUItranslator());

	// avail styles
	ui.styleComboBox->addItem(tr("System Predefined"));
	QStringList sl(QStyleFactory::keys());
	sl.sort();
	ui.styleComboBox->addItems(sl);
	ui.styleComboBox->setCurrentIndex(GUIstyle());
	ui.recentlyUsedSpinBox->setValue(recentlyUsedCount());

	ui.nullCheckBox->setChecked(useNullHighlight());
	ui.nullAliasEdit->setText(nullHighlightText());
	ui.nullBgButton->setPalette(nullHighlightColor());

	ui.blobCheckBox->setChecked(useBlobHighlight());
	ui.blobAliasEdit->setText(blobHighlightText());
	ui.blobBgButton->setPalette(blobHighlightColor());

	ui.cropColumnsCheckBox->setChecked(cropColumns());

	ui.fontComboBox->setCurrentFont(sqlFont());
	ui.useActiveHighlightCheckBox->setChecked(useActiveHighlighting());
	ui.activeHighlightButton->setPalette(activeHighlightColor());
	ui.useTextWidthMarkCheckBox->setChecked(useTextWidthMark());
	ui.textWidthMarkSpinBox->setValue(textWidthMark());
	ui.useCompletionCheck->setChecked(useCodeCompletion());
	ui.completionLengthBox->setValue(codeCompletionLength());
	ui.useShortcutsBox->setChecked(useShortcuts());
}

bool PreferencesDialog::saveSettings()
{
	QSettings settings("yarpen.cz", "sqliteman");

	// lnf
	settings.setValue("prefs/languageComboBox", ui.languageComboBox->currentIndex());
	settings.setValue("prefs/styleComboBox", ui.styleComboBox->currentIndex());
	settings.setValue("prefs/recentlyUsedSpinBox", ui.recentlyUsedSpinBox->value());
	// data results
	settings.setValue("prefs/nullCheckBox", ui.nullCheckBox->isChecked());
	settings.setValue("prefs/nullAliasEdit", ui.nullAliasEdit->text());
	settings.setValue("prefs/nullBgButton", ui.nullBgButton->palette().color(QPalette::Background));
	settings.setValue("prefs/blobCheckBox", ui.blobCheckBox->isChecked());
	settings.setValue("prefs/blobAliasEdit", ui.blobAliasEdit->text());
	settings.setValue("prefs/blobBgButton", ui.blobBgButton->palette().color(QPalette::Background));
	settings.setValue("prefs/cropColumnsCheckBox", ui.cropColumnsCheckBox->isChecked());
	// sql editor
	settings.setValue("prefs/sqleditor/font", ui.fontComboBox->currentFont());
	settings.setValue("prefs/sqleditor/useActiveHighlightCheckBox", ui.useActiveHighlightCheckBox->isChecked());
	settings.setValue("prefs/sqleditor/activeHighlightButton", ui.activeHighlightButton->palette().color(QPalette::Background));
	settings.setValue("prefs/sqleditor/useTextWidthMarkCheckBox", ui.useTextWidthMarkCheckBox->isChecked());
	settings.setValue("prefs/sqleditor/textWidthMarkSpinBox", ui.textWidthMarkSpinBox->value());
	settings.setValue("prefs/sqleditor/useCodeCompletion", ui.useCompletionCheck->isChecked());
	settings.setValue("prefs/sqleditor/completionLengthBox", ui.completionLengthBox->value());
	settings.setValue("prefs/sqleditor/useShortcuts", ui.useShortcutsBox->isChecked());

	if (settings.status() != QSettings::NoError)
		return false;
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

	ui.fontComboBox->setCurrentFont(QFont());
	ui.useActiveHighlightCheckBox->setChecked(true);
	ui.activeHighlightButton->setPalette(defCol);
	ui.useTextWidthMarkCheckBox->setChecked(true);
	ui.textWidthMarkSpinBox->setValue(75);
	ui.useCompletionCheck->setChecked(false);
	ui.completionLengthBox->setValue(3);
	ui.useShortcutsBox->setChecked(false);
	saveShortcuts(QMap<QString,QVariant>());
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

bool PreferencesDialog::useNullHighlight()
{
	QSettings s("yarpen.cz", "sqliteman");
	return s.value("prefs/nullCheckBox", true).toBool();
}

bool PreferencesDialog::useBlobHighlight()
{
	QSettings s("yarpen.cz", "sqliteman");
	return s.value("prefs/blobCheckBox", true).toBool();
}

QString PreferencesDialog::nullHighlightText()
{
	QSettings s("yarpen.cz", "sqliteman");
	return s.value("prefs/nullAliasEdit", "{null}").toString();
}

QString PreferencesDialog::blobHighlightText()
{
	QSettings s("yarpen.cz", "sqliteman");
	return s.value("prefs/blobAliasEdit", "{blob}").toString();
}

QColor PreferencesDialog::nullHighlightColor()
{
	QSettings s("yarpen.cz", "sqliteman");
	return s.value("prefs/nullBgButton", QColor(255, 254, 205)).value<QColor>();
}

QColor PreferencesDialog::blobHighlightColor()
{
	QSettings s("yarpen.cz", "sqliteman");
	return s.value("prefs/blobBgButton", QColor(255, 254, 205)).value<QColor>();
}

int PreferencesDialog::recentlyUsedCount()
{
	QSettings s("yarpen.cz", "sqliteman");
	return s.value("prefs/recentlyUsedSpinBox", 5).toInt();
}

int PreferencesDialog::GUItranslator()
{
	QSettings s("yarpen.cz", "sqliteman");
	return s.value("prefs/languageComboBox", 0).toInt();
}

int PreferencesDialog::GUIstyle()
{
	QSettings s("yarpen.cz", "sqliteman");
	return s.value("prefs/styleComboBox", 0).toInt();
}

bool PreferencesDialog::cropColumns()
{
	QSettings s("yarpen.cz", "sqliteman");
	return s.value("prefs/cropColumnsCheckBox", false).toBool();
}

QFont PreferencesDialog::sqlFont()
{
	QSettings s("yarpen.cz", "sqliteman");
	return s.value("prefs/sqleditor/font", QFont()).value<QFont>();
}

bool PreferencesDialog::useActiveHighlighting()
{
	QSettings s("yarpen.cz", "sqliteman");
	return s.value("prefs/sqleditor/useActiveHighlightCheckBox", true).toBool();
}

QColor PreferencesDialog::activeHighlightColor()
{
	QSettings s("yarpen.cz", "sqliteman");
	return s.value("prefs/sqleditor/activeHighlightButton", QColor(255, 254, 205)).value<QColor>();
}

bool PreferencesDialog::useTextWidthMark()
{
	QSettings s("yarpen.cz", "sqliteman");
	return s.value("prefs/sqleditor/useTextWidthMarkCheckBox", true).toBool();
}

int PreferencesDialog::textWidthMark()
{
	QSettings s("yarpen.cz", "sqliteman");
	return s.value("prefs/sqleditor/textWidthMarkSpinBox", 80).toInt();
}

bool PreferencesDialog::useCodeCompletion()
{
	QSettings s("yarpen.cz", "sqliteman");
	return s.value("prefs/sqleditor/useCodeCompletion", false).toBool();
}

int PreferencesDialog::codeCompletionLength()
{
	QSettings s("yarpen.cz", "sqliteman");
	return s.value("prefs/sqleditor/completionLengthBox", 3).toInt();
}

bool PreferencesDialog::useShortcuts()
{
	QSettings s("yarpen.cz", "sqliteman");
	return s.value("prefs/sqleditor/useShortcuts", false).toBool();
}

QMap<QString,QVariant> PreferencesDialog::shortcuts()
{
	QSettings s("yarpen.cz", "sqliteman");
	return s.value("prefs/sqleditor/shortcuts", QMap<QString,QVariant>()).toMap();
}

bool PreferencesDialog::saveShortcuts(QMap<QString,QVariant> map)
{
	QSettings s("yarpen.cz", "sqliteman");
	s.setValue("prefs/sqleditor/shortcuts", map);
	if (s.status() != QSettings::NoError)
		return false;
	return true;
}
