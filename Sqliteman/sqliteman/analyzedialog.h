/*
For general Sqliteman copyright and licensing information please refer
to the COPYING file provided with the program. Following this notice may exist
a copyright and/or license notice that predates the release of Sqliteman
for which a new license (GPL+exception) is in place.
*/

#ifndef ANALYZEDIALOG_H
#define ANALYZEDIALOG_H

#include <qdialog.h>

#include "ui_analyzedialog.h"


/*! \brief Handle DB statistics here.
Sqlite3 offers simple statistics for its internal SQL optimizer.
\author Petr Vanek <petr@scribus.info>
 */
class AnalyzeDialog : public QDialog
{
	Q_OBJECT

	public:
		AnalyzeDialog(QWidget * parent = 0);
		~AnalyzeDialog(){};

	private:
		Ui::AnalyzeDialog ui;

    private slots:
		void dropButton_clicked();
		void allButton_clicked();
		void tableButton_clicked();
};

#endif
