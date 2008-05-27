/*
For general Sqliteman copyright and licensing information please refer
to the COPYING file provided with the program. Following this notice may exist
a copyright and/or license notice that predates the release of Sqliteman
for which a new license (GPL+exception) is in place.
*/

#ifndef PREFERENCES_H
#define PREFERENCES_H

#include <QObject>
#include <QMap>
#include <QColor>
#include <QFont>
#include <QVariant>


class Preferences : public QObject
{
	Q_OBJECT

	public:
		Preferences(QObject *parent = 0);
		~Preferences();

		static Preferences* instance();
		static void deleteInstance();

		static QColor stdLightColor() { return QColor(255, 254, 205); };
		static QColor stdDarkColor() { return QColor(225, 237, 255); };

		bool checkQtVersion() { return m_checkQtVersion; };

		bool nullHighlight() { return m_nullHighlight; };
		void setNullHighlight(bool v) { m_nullHighlight = v; };

		bool blobHighlight() { return m_blobHighlight; };
		void setBlobHighlight(bool v) { m_blobHighlight = v; };

		QString nullHighlightText() { return m_nullHighlightText; };
		void setNullHighlightText(const QString & v) { m_nullHighlightText = v; };

		QString blobHighlightText() { return m_blobHighlightText; };
		void setBlobHighlightText(const QString & v) { m_blobHighlightText = v; };

		QColor nullHighlightColor() { return m_nullHighlightColor; };
		void setNullHighlightColor(const QColor & v) { m_nullHighlightColor = v; };

		QColor blobHighlightColor() { return m_blobHighlightColor; };
		void setBlobHighlightColor(const QColor & v) { m_blobHighlightColor = v; };

		int recentlyUsedCount() { return m_recentlyUsedCount; };
		void setRecentlyUsedCount(int v) { m_recentlyUsedCount = v; };

		bool openLastDB() { return m_openLastDB; };
		bool openLastSqlFile() { return m_openLastSqlFile; };
		void setOpenLastDB(bool v) { m_openLastDB = v; };
		void setOpenLastSqlFile(bool v) { m_openLastSqlFile = v; };
		QString lastDB() { return m_lastDB; };

		int GUItranslator() { return m_GUItranslator; };
		void setGUItranslator(int v) { m_GUItranslator = v; };

		int GUIstyle() { return m_GUIstyle; };
		void setGUIstyle(int v) { m_GUIstyle = v; };

		bool cropColumns() { return m_cropColumns; };
		void setCropColumns(bool v) { m_cropColumns = v; };

		QFont sqlFont() { return m_sqlFont; };
		void setSqlFont(QFont v) { m_sqlFont = v; };

		int sqlFontSize() { return m_sqlFontSize; };
		void setSqlFontSize(int v) { m_sqlFontSize = v; };

		bool activeHighlighting() { return m_activeHighlighting; };
		void setActiveHighlighting(bool v) { m_activeHighlighting = v; };

		QColor activeHighlightColor() { return m_activeHighlightColor; };
		void setActiveHighlightColor(const QColor & v) { m_activeHighlightColor = v; };

		bool textWidthMark() { return m_textWidthMark; };
		void setTextWidthMark(bool v) { m_textWidthMark = v; };

		int textWidthMarkSize() { return m_textWidthMarkSize; };
		void setTextWidthMarkSize(int v) { m_textWidthMarkSize = v; };

		bool codeCompletion() { return m_codeCompletion; };
		void setCodeCompletion(bool v) { m_codeCompletion = v; };

		int codeCompletionLength() { return m_codeCompletionLength; };
		void setCodeCompletionLength(int v) { m_codeCompletionLength = v; };

		bool useShortcuts() { return m_useShortcuts; };
		void setUseShortcuts(bool v) { m_useShortcuts = v; };

		QMap<QString,QVariant> shortcuts() { return m_shortcuts; };
		void setShortcuts(QMap<QString,QVariant>  v) { m_shortcuts = v; };

		QString dateTimeFormat() { return m_dateTimeFormat; };
		void setDateTimeFormat(const QString & v) { m_dateTimeFormat = v; };

		// data export
		int exportFormat() { return m_exportFormat; };
		void setExportFormat(int v) { m_exportFormat = v; };

		int exportDestination() { return m_exportDestination; };
		void setExportDestination(int v) { m_exportDestination = v; };

		bool exportHeaders() { return m_exportHeaders; };
		void setExportHeaders(bool v) { m_exportHeaders = v; };

		QString exportEncoding() { return m_exportEncoding; };
		void setExportEncoding(const QString & v) { m_exportEncoding = v; };

		int exportEol() { return m_exportEol; };
		void setExportEol(int v) { m_exportEol = v; };

		// qscintilla syntax
		QColor syDefaultColor() { return m_syDefaultColor; };
		void setSyDefaultColor(const QColor & v ) { m_syDefaultColor = v; };

		QColor syKeywordColor() { return m_syKeywordColor; };
		void setSyKeywordColor(const QColor & v ) { m_syKeywordColor = v; };

		QColor syNumberColor() { return m_syNumberColor; };
		void setSyNumberColor(const QColor & v ) { m_syNumberColor = v; };

		QColor syStringColor() { return m_syStringColor; };
		void setSyStringColor(const QColor & v ) { m_syStringColor = v; };

		QColor syCommentColor() { return m_syCommentColor; };
		void setSyCommentColor(const QColor & v ) { m_syCommentColor = v; };

	signals:
		void prefsChanged();

	private:
		/*! \brief The only instance of PrefsManager available.
		Preferences is singleton and the instance can be queried with the method
		instance().
		*/
		static Preferences* _instance;

		bool m_checkQtVersion;
		bool m_nullHighlight;
		bool m_blobHighlight;
		QString m_nullHighlightText;
		QString m_blobHighlightText;
		QColor m_nullHighlightColor;
		QColor m_blobHighlightColor;
		int m_recentlyUsedCount;
		bool m_openLastDB;
		bool m_openLastSqlFile;
		QString m_lastDB;
		int m_GUItranslator;
		int m_GUIstyle;
		bool m_cropColumns;
		QFont m_sqlFont;
		int m_sqlFontSize;
		bool m_activeHighlighting;
		QColor m_activeHighlightColor;
		bool m_textWidthMark;
		int m_textWidthMarkSize;
		bool m_codeCompletion;
		int m_codeCompletionLength;
		bool m_useShortcuts;
		QMap<QString,QVariant> m_shortcuts;
		// qscintilla syntax
		QColor m_syDefaultColor;
		QColor m_syKeywordColor;
		QColor m_syNumberColor;
		QColor m_syStringColor;
		QColor m_syCommentColor;
		// data export
		int m_exportFormat;
		int m_exportDestination;
		bool m_exportHeaders;
		QString m_exportEncoding;
		int m_exportEol;

		// used in MultieditDialog
		QString m_dateTimeFormat;
};

#endif
