/*
For general Sqliteman copyright and licensing information please refer
to the COPYING file provided with the program. Following this notice may exist
a copyright and/or license notice that predates the release of Sqliteman
for which a new license (GPL+exception) is in place.
*/
#include <QSettings>
#include <QApplication>
#include <qscilexersql.h>

#include "preferences.h"


Preferences* Preferences::_instance = 0;


Preferences::Preferences(QObject *parent)
 : QObject(parent)
{
	QSettings s("yarpen.cz", "sqliteman");
	m_checkQtVersion = s.value("checkQtVersion", true).toBool();
	//
	m_nullHighlight = s.value("prefs/nullCheckBox", true).toBool();
	m_blobHighlight = s.value("prefs/blobCheckBox", true).toBool();
	m_nullHighlightText = s.value("prefs/nullAliasEdit", "{null}").toString();
	m_blobHighlightText = s.value("prefs/blobAliasEdit", "{blob}").toString();
	m_nullHighlightColor = s.value("prefs/nullBgButton", stdLightColor()).value<QColor>();
	m_blobHighlightColor = s.value("prefs/blobBgButton", stdLightColor()).value<QColor>();
	m_recentlyUsedCount = s.value("prefs/recentlyUsedSpinBox", 5).toInt();
	m_openLastDB = s.value("prefs/openLastDB", true).toBool();
	m_openLastSqlFile = s.value("prefs/openLastSqlFile", true).toBool();
	m_lastDB = s.value("lastDatabase", QString()).toString();
	m_GUItranslator = s.value("prefs/languageComboBox", 0).toInt();
	m_GUIstyle = s.value("prefs/styleComboBox", 0).toInt();
	m_cropColumns = s.value("prefs/cropColumnsCheckBox", false).toBool();
	QFont f(QApplication::font());
// 	f.setPointSize(sqlFontSize());
	m_sqlFont = s.value("prefs/sqleditor/font", f).value<QFont>();
	m_sqlFontSize = s.value("prefs/sqleditor/fontSize", f.pointSize()).toInt();
	m_activeHighlighting = s.value("prefs/sqleditor/useActiveHighlightCheckBox", true).toBool();
	m_activeHighlightColor = s.value("prefs/sqleditor/activeHighlightButton", stdDarkColor()).value<QColor>();
	m_textWidthMark = s.value("prefs/sqleditor/useTextWidthMarkCheckBox", true).toBool();
	m_textWidthMarkSize = s.value("prefs/sqleditor/textWidthMarkSpinBox", 60).toInt();
	m_codeCompletion = s.value("prefs/sqleditor/useCodeCompletion", false).toBool();
	m_codeCompletionLength = s.value("prefs/sqleditor/completionLengthBox", 3).toInt();
	m_useShortcuts = s.value("prefs/sqleditor/useShortcuts", false).toBool();
	m_shortcuts = s.value("prefs/sqleditor/shortcuts", QMap<QString,QVariant>()).toMap();
	// qscintilla
	QsciLexerSQL syntaxLexer;
	m_syDefaultColor = s.value("prefs/qscintilla/syDefaultColor",
							   syntaxLexer.defaultColor(QsciLexerSQL::Default)).value<QColor>();
	m_syKeywordColor = s.value("prefs/qscintilla/syKeywordColor",
							   syntaxLexer.defaultColor(QsciLexerSQL::Keyword)).value<QColor>();
	m_syNumberColor = s.value("prefs/qscintilla/syNumberColor",
							  syntaxLexer.defaultColor(QsciLexerSQL::Number)).value<QColor>();
	m_syStringColor = s.value("prefs/qscintilla/syStringColor",
							  syntaxLexer.defaultColor(QsciLexerSQL::SingleQuotedString)).value<QColor>();
	m_syCommentColor = s.value("prefs/qscintilla/syCommentColor",
							   syntaxLexer.defaultColor(QsciLexerSQL::Comment)).value<QColor>();
	// data
	m_dateTimeFormat = s.value("data/dateTimeFormat", "MM/dd/yyyy").toString();
	// data export
	m_exportFormat = s.value("dataExport/format", 0).toInt();
	m_exportDestination = s.value("dataExport/destination", 0).toInt();
	m_exportHeaders = s.value("dataExport/headers", true).toBool();
	m_exportEncoding = s.value("dataExport/encoding", "UTF-8").toString();
	m_exportEol = s.value("dataExport/eol", 0).toInt();
}

Preferences::~Preferences()
{
	QSettings settings("yarpen.cz", "sqliteman");
	settings.setValue("checkQtVersion", m_checkQtVersion);
	// lnf
	settings.setValue("prefs/languageComboBox", m_GUItranslator);
	settings.setValue("prefs/styleComboBox", m_GUIstyle);
	settings.setValue("prefs/recentlyUsedSpinBox", m_recentlyUsedCount);
	settings.setValue("prefs/openLastDB", m_openLastDB);
	settings.setValue("prefs/openLastSqlFile", m_openLastSqlFile);
	// data results
	settings.setValue("prefs/nullCheckBox", m_nullHighlight);
	settings.setValue("prefs/nullAliasEdit", m_nullHighlightText);
	settings.setValue("prefs/nullBgButton", m_nullHighlightColor);
	settings.setValue("prefs/blobCheckBox", m_blobHighlight);
	settings.setValue("prefs/blobAliasEdit", m_blobHighlightText);
	settings.setValue("prefs/blobBgButton", m_blobHighlightColor);
	settings.setValue("prefs/cropColumnsCheckBox", m_cropColumns);
	// sql editor
	settings.setValue("prefs/sqleditor/font", m_sqlFont);
    settings.setValue("prefs/sqleditor/fontSize", m_sqlFontSize);
	settings.setValue("prefs/sqleditor/useActiveHighlightCheckBox", m_activeHighlighting);
	settings.setValue("prefs/sqleditor/activeHighlightButton", m_activeHighlightColor);
	settings.setValue("prefs/sqleditor/useTextWidthMarkCheckBox", m_textWidthMark);
	settings.setValue("prefs/sqleditor/textWidthMarkSpinBox", m_textWidthMarkSize);
	settings.setValue("prefs/sqleditor/useCodeCompletion", m_codeCompletion);
	settings.setValue("prefs/sqleditor/completionLengthBox", m_codeCompletionLength);
	settings.setValue("prefs/sqleditor/useShortcuts", m_useShortcuts);
	settings.setValue("prefs/sqleditor/shortcuts", m_shortcuts);
	// qscintilla editor
	settings.setValue("prefs/qscintilla/syDefaultColor", m_syDefaultColor);
	settings.setValue("prefs/qscintilla/syKeywordColor", m_syKeywordColor);
	settings.setValue("prefs/qscintilla/syNumberColor", m_syNumberColor);
	settings.setValue("prefs/qscintilla/syStringColor", m_syStringColor);
	settings.setValue("prefs/qscintilla/syCommentColor", m_syCommentColor);
	//
	settings.setValue("data/dateTimeFormat", m_dateTimeFormat);
	// data export
	settings.setValue("dataExport/format", m_exportFormat);
	settings.setValue("dataExport/destination", m_exportDestination);
	settings.setValue("dataExport/headers", m_exportHeaders);
	settings.setValue("dataExport/encoding", m_exportEncoding);
	settings.setValue("dataExport/eol", m_exportEol);
}

Preferences* Preferences::instance()
{
    if (_instance == 0)
        _instance = new Preferences();

    return _instance;
}

void Preferences::deleteInstance()
{
    if (_instance)
        delete _instance;
    _instance = 0;
}
