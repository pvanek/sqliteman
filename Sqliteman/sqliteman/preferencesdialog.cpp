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


PreferencesDialog::PreferencesDialog(QWidget * parent)
	: QDialog(parent)
{
	ui.setupUi(this);

	connect(ui.nullCheckBox, SIGNAL(stateChanged(int)), this, SLOT(nullCheckBox_stateChanged(int)));
	connect(ui.blobCheckBox, SIGNAL(stateChanged(int)), this, SLOT(blobCheckBox_stateChanged(int)));
	connect(ui.nullBgButton, SIGNAL(clicked()), this, SLOT(nullBgButton_clicked()));
	connect(ui.blobBgButton, SIGNAL(clicked()), this, SLOT(blobBgButton_clicked()));
	connect(ui.activeHighlightButton, SIGNAL(clicked()), this, SLOT(activeHighlightButton_clicked()));
	connect(ui.buttonBox->button(QDialogButtonBox::RestoreDefaults), SIGNAL(clicked()), this, SLOT(restoreDefaults()));

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

	ui.nullCheckBox->setChecked(useNullHighlight());
	ui.nullAliasEdit->setText(nullHighlightText());
	ui.nullBgButton->setPalette(nullHighlightColor());

	ui.blobCheckBox->setChecked(useBlobHighlight());
	ui.blobAliasEdit->setText(blobHighlightText());
	ui.blobBgButton->setPalette(blobHighlightColor());

	ui.recentlyUsedSpinBox->setValue(recentlyUsedCount());

	ui.fontComboBox->setCurrentFont(sqlFont());
	ui.useActiveHighlightCheckBox->setChecked(useActiveHighlighting());
	ui.activeHighlightButton->setPalette(activeHighlightColor());
	ui.useTextWidthMarkCheckBox->setChecked(useTextWidthMark());
	ui.textWidthMarkSpinBox->setValue(textWidthMark());
}

bool PreferencesDialog::saveSettings()
{
	QSettings settings("yarpen.cz", "sqliteman");

	// lnf
	settings.setValue("prefs/languageComboBox", ui.languageComboBox->currentIndex());
	settings.setValue("prefs/styleComboBox", ui.styleComboBox->currentIndex());
	// data results
	settings.setValue("prefs/nullCheckBox", ui.nullCheckBox->isChecked());
	settings.setValue("prefs/nullAliasEdit", ui.nullAliasEdit->text());
	settings.setValue("prefs/nullBgButton", ui.nullBgButton->palette().color(QPalette::Background));
	settings.setValue("prefs/blobCheckBox", ui.blobCheckBox->isChecked());
	settings.setValue("prefs/blobAliasEdit", ui.blobAliasEdit->text());
	settings.setValue("prefs/blobBgButton", ui.blobBgButton->palette().color(QPalette::Background));
	settings.setValue("prefs/recentlyUsedSpinBox", ui.recentlyUsedSpinBox->value());
	// sql editor
	settings.setValue("prefs/sqleditor/font", ui.fontComboBox->currentFont());
	settings.setValue("prefs/sqleditor/useActiveHighlightCheckBox", ui.useActiveHighlightCheckBox->isChecked());
	settings.setValue("prefs/sqleditor/activeHighlightButton", ui.activeHighlightButton->palette().color(QPalette::Background));
	settings.setValue("prefs/sqleditor/useTextWidthMarkCheckBox", ui.useTextWidthMarkCheckBox->isChecked());
	settings.setValue("prefs/sqleditor/textWidthMarkSpinBox", ui.textWidthMarkSpinBox->value());

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

	ui.fontComboBox->setCurrentFont(QFont());
	ui.useActiveHighlightCheckBox->setChecked(true);
	ui.activeHighlightButton->setPalette(defCol);
	ui.useTextWidthMarkCheckBox->setChecked(true);
	ui.textWidthMarkSpinBox->setValue(75);
}

void PreferencesDialog::nullCheckBox_stateChanged(int)
{
	ui.nullAliasEdit->setEnabled(ui.nullCheckBox->isChecked());
	ui.nullBgButton->setEnabled(ui.nullCheckBox->isChecked());
}

void PreferencesDialog::blobCheckBox_stateChanged(int)
{
	ui.blobAliasEdit->setEnabled(ui.blobCheckBox->isChecked());
	ui.blobBgButton->setEnabled(ui.blobCheckBox->isChecked());
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
