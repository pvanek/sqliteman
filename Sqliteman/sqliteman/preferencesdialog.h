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
#include "ui_prefsdatadisplaywidget.h"
#include "ui_prefslnfwidget.h"
#include "ui_prefssqleditorwidget.h"


class PrefsDataDisplayWidget : public QWidget, public Ui::PrefsDataDisplayWidget
{
	Q_OBJECT
	public:
		PrefsDataDisplayWidget(QWidget * parent = 0);
};

class PrefsLNFWidget : public QWidget, public Ui::PrefsLNFWidget
{
	Q_OBJECT
	public:
		PrefsLNFWidget(QWidget * parent = 0);
};

class PrefsSQLEditorWidget : public QWidget, public Ui::PrefsSQLEditorWidget
{
	Q_OBJECT
	public:
		PrefsSQLEditorWidget(QWidget * parent = 0);
};


/*! \brief Basic preferences dialog and handling.
It constructs GUI to manage the prefs. The static methods
are used to access the prefs out of this class in the
application guts.
\author Petr Vanek <petr@scribus.info>
*/
class PreferencesDialog : public QDialog, public Ui::PreferencesDialog
{
	Q_OBJECT

	public:
		PreferencesDialog(QWidget * parent = 0);
		~PreferencesDialog(){};

		bool saveSettings();

	private:
		PrefsDataDisplayWidget * m_prefsData;
		PrefsLNFWidget * m_prefsLNF;
		PrefsSQLEditorWidget * m_prefsSQL;

		// temporary qscintilla syntax colors
		QColor m_syDefaultColor;
		QColor m_syKeywordColor;
		QColor m_syNumberColor;
		QColor m_syStringColor;
		QColor m_syCommentColor;

		//! \brief Update editor preview for new color/font values.
		void resetEditorPreview();

	private slots:
		void restoreDefaults();
		void blobBgButton_clicked();
		void nullBgButton_clicked();
		void activeHighlightButton_clicked();
		void shortcutsButton_clicked();
		//
		void syDefaultButton_clicked();
		void syKeywordButton_clicked();
		void syNumberButton_clicked();
		void syStringButton_clicked();
		void syCommentButton_clicked();
		//
		void fontComboBox_activated(int);
		void fontSizeSpin_valueChanged(int);
};


#endif
