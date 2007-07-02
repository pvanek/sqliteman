/*
For general Sqliteman copyright and licensing information please refer
to the COPYING file provided with the program. Following this notice may exist
a copyright and/or license notice that predates the release of Sqliteman
for which a new license (GPL+exception) is in place.
*/

#ifndef HELPBROWSER_H
#define HELPBROWSER_H

#include <QMainWindow>
#include "ui_helpbrowser.h"


class HelpBrowser : public QMainWindow
{
	Q_OBJECT

	public:
		HelpBrowser(const QString & lang, QWidget * parent = 0);
		~HelpBrowser(){};

	private:
		Ui::HelpBrowser ui;
		void closeEvent(QCloseEvent *e);
		void setHistoryButtonsState();

	private slots:
		void menuBrowser_anchorClicked(const QUrl &);
		void forward();
		void backward();
};

#endif
