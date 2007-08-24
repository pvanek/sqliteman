/*
For general Sqliteman copyright and licensing information please refer
to the COPYING file provided with the program. Following this notice may exist
a copyright and/or license notice that predates the release of Sqliteman
for which a new license (GPL+exception) is in place.
*/

#ifndef IMPORTTABLEDIALOG_H
#define IMPORTTABLEDIALOG_H

#include "ui_importtabledialog.h"


/*! \brief Import data into table using .import
\author Petr Vanek <petr@scribus.info>
*/
class ImportTableDialog : public QDialog, public Ui::ImportTableDialog
{
	Q_OBJECT

	public:
		ImportTableDialog(QWidget * parent = 0, const QString & tableName = 0, const QString & schema = 0);

	private:
		QObject * m_parent;
		bool sqliteImport();

	private slots:
		void fileButton_clicked();
		void slotAccepted();

};

#endif
