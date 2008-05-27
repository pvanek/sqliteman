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
#include <qscilexersql.h>

#include "preferencesdialog.h"
#include "preferences.h"
#include "shortcuteditordialog.h"
#include "utils.h"


PrefsDataDisplayWidget::PrefsDataDisplayWidget(QWidget * parent)
	: QWidget(parent)
{
	setupUi(this);
}

PrefsLNFWidget::PrefsLNFWidget(QWidget * parent)
	: QWidget(parent)
{
	setupUi(this);
}

PrefsSQLEditorWidget::PrefsSQLEditorWidget(QWidget * parent)
	: QWidget(parent)
{
	setupUi(this);

#if QT_VERSION < 0x040300
	useShortcutsBox->hide();
	shortcutsButton->hide();
#endif

	syntaxPreviewEdit->setText("-- this is a comment\n" \
			"select *\n"
			"from table_name\n" \
			"  where id > 37\n"\
			"    and text_column\n"
			"      like '%foo%';\n");
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

	listWidget->addItem(new QListWidgetItem(Utils::getIcon("preferences-desktop-display.png"),
						m_prefsLNF->titleLabel->text(), listWidget));
	listWidget->addItem(new QListWidgetItem(Utils::getIcon("table.png"),
						m_prefsData->titleLabel->text(), listWidget));
	listWidget->addItem(new QListWidgetItem(Utils::getIcon("kate.png"),
						m_prefsSQL->titleLabel->text(), listWidget));
	listWidget->setCurrentRow(0);

	connect(m_prefsData->nullBgButton, SIGNAL(clicked()),
			this, SLOT(nullBgButton_clicked()));
	connect(m_prefsData->blobBgButton, SIGNAL(clicked()),
			this, SLOT(blobBgButton_clicked()));
	connect(m_prefsSQL->activeHighlightButton, SIGNAL(clicked()),
			this, SLOT(activeHighlightButton_clicked()));
	connect(buttonBox->button(QDialogButtonBox::RestoreDefaults),
			SIGNAL(clicked()), this, SLOT(restoreDefaults()));
	connect(m_prefsSQL->fontComboBox, SIGNAL(activated(int)),
			this, SLOT(fontComboBox_activated(int)));
	connect(m_prefsSQL->fontSizeSpin, SIGNAL(valueChanged(int)),
			this, SLOT(fontSizeSpin_valueChanged(int)));
	connect(m_prefsSQL->shortcutsButton, SIGNAL(clicked()),
			this, SLOT(shortcutsButton_clicked()));
	// qscintilla syntax colors
	connect(m_prefsSQL->syDefaultButton, SIGNAL(clicked()),
			this, SLOT(syDefaultButton_clicked()));
	connect(m_prefsSQL->syKeywordButton, SIGNAL(clicked()),
			this, SLOT(syKeywordButton_clicked()));
	connect(m_prefsSQL->syNumberButton, SIGNAL(clicked()),
			this, SLOT(syNumberButton_clicked()));
	connect(m_prefsSQL->syStringButton, SIGNAL(clicked()),
			this, SLOT(syStringButton_clicked()));
	connect(m_prefsSQL->syCommentButton, SIGNAL(clicked()),
			this, SLOT(syCommentButton_clicked()));
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
	m_prefsLNF->openLastDBCheckBox->setChecked(prefs->openLastDB());
	m_prefsLNF->openLastSqlFileCheckBox->setChecked(prefs->openLastSqlFile());

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

	m_syDefaultColor = prefs->syDefaultColor();
	m_syKeywordColor = prefs->syKeywordColor();
	m_syNumberColor = prefs->syNumberColor();
	m_syStringColor = prefs->syStringColor();
	m_syCommentColor = prefs->syCommentColor();
	resetEditorPreview();
}

bool PreferencesDialog::saveSettings()
{
	Preferences * prefs = Preferences::instance();
	prefs->setGUItranslator(m_prefsLNF->languageComboBox->currentIndex());
	prefs->setGUIstyle(m_prefsLNF->styleComboBox->currentIndex());
	prefs->setRecentlyUsedCount(m_prefsLNF->recentlyUsedSpinBox->value());
	prefs->setOpenLastDB(m_prefsLNF->openLastDBCheckBox->isChecked());
	prefs->setOpenLastSqlFile(m_prefsLNF->openLastSqlFileCheckBox->isChecked());
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
	// qscintilla
	prefs->setSyDefaultColor(m_syDefaultColor);
	prefs->setSyKeywordColor(m_syKeywordColor);
	prefs->setSyNumberColor(m_syNumberColor);
	prefs->setSyStringColor(m_syStringColor);
	prefs->setSyCommentColor(m_syCommentColor);

	return true;
}

void PreferencesDialog::restoreDefaults()
{
	m_prefsLNF->languageComboBox->setCurrentIndex(0);
	m_prefsLNF->styleComboBox->setCurrentIndex(0);
	m_prefsLNF->recentlyUsedSpinBox->setValue(5);
	m_prefsLNF->openLastDBCheckBox->setChecked(true);
	m_prefsLNF->openLastSqlFileCheckBox->setChecked(true);

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
	//
	QsciLexerSQL syntaxLexer;
	m_syDefaultColor = syntaxLexer.defaultColor(QsciLexerSQL::Default);
	m_syKeywordColor = syntaxLexer.defaultColor(QsciLexerSQL::Keyword);
	m_syNumberColor = syntaxLexer.defaultColor(QsciLexerSQL::Number);
	m_syStringColor = syntaxLexer.defaultColor(QsciLexerSQL::SingleQuotedString);
	m_syCommentColor = syntaxLexer.defaultColor(QsciLexerSQL::Comment);

	resetEditorPreview();
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
	{
		m_prefsSQL->activeHighlightButton->setPalette(nCol);
		resetEditorPreview();
	}
}

void PreferencesDialog::shortcutsButton_clicked()
{
	ShortcutEditorDialog d;
	d.exec();
}

void PreferencesDialog::syDefaultButton_clicked()
{
	QColor nCol = QColorDialog::getColor(m_syDefaultColor, this);
	if (nCol.isValid())
	{
		m_syDefaultColor = nCol;
		resetEditorPreview();
	}
}

void PreferencesDialog::syKeywordButton_clicked()
{
	QColor nCol = QColorDialog::getColor(m_syKeywordColor, this);
	if (nCol.isValid())
	{
		m_syKeywordColor = nCol;
		resetEditorPreview();
	}
}

void PreferencesDialog::syNumberButton_clicked()
{
	QColor nCol = QColorDialog::getColor(m_syNumberColor, this);
	if (nCol.isValid())
	{
		m_syNumberColor = nCol;
		resetEditorPreview();
	}
}

void PreferencesDialog::syStringButton_clicked()
{
	QColor nCol = QColorDialog::getColor(m_syStringColor, this);
	if (nCol.isValid())
	{
		m_syStringColor = nCol;
		resetEditorPreview();
	}
}

void PreferencesDialog::syCommentButton_clicked()
{
	QColor nCol = QColorDialog::getColor(m_syCommentColor, this);
	if (nCol.isValid())
	{
		m_syCommentColor = nCol;
		resetEditorPreview();
	}
}

void PreferencesDialog::fontComboBox_activated(int)
{
	resetEditorPreview();
}

void PreferencesDialog::fontSizeSpin_valueChanged(int)
{
	resetEditorPreview();
}

void PreferencesDialog::resetEditorPreview()
{
	QsciLexerSQL *lexer = qobject_cast<QsciLexerSQL*>(m_prefsSQL->syntaxPreviewEdit->lexer());

	QFont newFont(m_prefsSQL->fontComboBox->currentFont());
	newFont.setPointSize(m_prefsSQL->fontSizeSpin->value());
	lexer->setFont(newFont);

	lexer->setColor(m_syDefaultColor, QsciLexerSQL::Default);
	lexer->setColor(m_syKeywordColor, QsciLexerSQL::Keyword);
	QFont defFont(lexer->font(QsciLexerSQL::Keyword));
	defFont.setBold(true);
	lexer->setFont(defFont, QsciLexerSQL::Keyword);
	lexer->setColor(m_syNumberColor, QsciLexerSQL::Number);
	lexer->setColor(m_syStringColor, QsciLexerSQL::SingleQuotedString);
	lexer->setColor(m_syStringColor, QsciLexerSQL::DoubleQuotedString);
	lexer->setColor(m_syCommentColor, QsciLexerSQL::Comment);
	lexer->setColor(m_syCommentColor, QsciLexerSQL::CommentLine);
	lexer->setColor(m_syCommentColor, QsciLexerSQL::CommentDoc);
}
