/*
For general Sqliteman copyright and licensing information please refer
to the COPYING file provided with the program. Following this notice may exist
a copyright and/or license notice that predates the release of Sqliteman
for which a new license (GPL+exception) is in place.
*/

#ifndef PREFERENCESDIALOG_H
#define PREFERENCESDIALOG_H

#include <QDialog>

#include "ui_preferencesdialog.h"


/*! \brief Basic preferences dialog and handling.
It constructs GUI to manage the prefs. The static methods
are used to access the prefs out of this class in the
application guts.
\author Petr Vanek <petr@scribus.info>
*/
class PreferencesDialog : public QDialog//, public Ui::PreferencesDialog
{
	Q_OBJECT

	public:
		PreferencesDialog(QWidget * parent = 0);
		~PreferencesDialog(){};

		bool saveSettings();

		static bool useNullHighlight();
		static bool useBlobHighlight();
		static QString nullHighlightText();
		static QString blobHighlightText();
		static QColor nullHighlightColor();
		static QColor blobHighlightColor();
		static int recentlyUsedCount();
		static int GUItranslator();
		static int GUIstyle();

		static QFont sqlFont();
		static bool useActiveHighlighting();
		static QColor activeHighlightColor();
		static bool useTextWidthMark();
		static int textWidthMark();
		static bool useCodeCompletion();
		static int codeCompletionLength();

		static bool useShortcuts();
		static QMap<QString,QVariant> shortcuts();
		static bool saveShortcuts(QMap<QString,QVariant> map);

	private:
		Ui::PreferencesDialog ui;

	private slots:
		void restoreDefaults();
		void blobBgButton_clicked();
		void nullBgButton_clicked();
		void activeHighlightButton_clicked();
		void shortcutsButton_clicked();
};


#endif
