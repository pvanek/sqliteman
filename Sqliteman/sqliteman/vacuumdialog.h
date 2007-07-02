/*
For general Sqliteman copyright and licensing information please refer
to the COPYING file provided with the program. Following this notice may exist
a copyright and/or license notice that predates the release of Sqliteman
for which a new license (GPL+exception) is in place.
*/

#ifndef VACUUMDIALOG_H
#define VACUUMDIALOG_H

#include <qdialog.h>

#include "ui_vacuumdialog.h"

/*! \brief Handle DB file (un)used space.
Sqlite3 offers VACUUM option for its internal data structures.
\author Petr Vanek <petr@scribus.info>
 */
class VacuumDialog : public QDialog
{
	Q_OBJECT

	public:
		VacuumDialog(QWidget * parent = 0);
		~VacuumDialog(){};

	private:
		Ui::VacuumDialog ui;

    private slots:
		void allButton_clicked();
		void tableButton_clicked();
};

#endif
