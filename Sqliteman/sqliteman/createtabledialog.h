/*
For general Sqliteman copyright and licensing information please refer
to the COPYING file provided with the program. Following this notice may exist
a copyright and/or license notice that predates the release of Sqliteman
for which a new license (GPL+exception) is in place.
*/

#ifndef CREATETABLEDIALOG_H
#define CREATETABLEDIALOG_H

#include "tableeditordialog.h"


/*! \brief A GUI for CREATE TABLE procedure.
\author Petr Vanek <petr@scribus.info>
*/
class CreateTableDialog : public TableEditorDialog
{
	Q_OBJECT

	public:
		CreateTableDialog(QWidget * parent = 0);
		~CreateTableDialog(){};

		bool update;

	private slots:
		void createButton_clicked();
		void tabWidget_currentChanged(int index);

	private:
		/*! \brief Analyze user changes and performs the CREATE TABLE sql
		*/
		QString getSQLfromGUI();
};

#endif
