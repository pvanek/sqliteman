/*
For general Sqliteman copyright and licensing information please refer
to the COPYING file provided with the program. Following this notice may exist
a copyright and/or license notice that predates the release of Sqliteman
for which a new license (GPL+exception) is in place.
*/

#ifndef CONSTRAINTSDIALOG_H
#define CONSTRAINTSDIALOG_H

#include <qdialog.h>

#include "ui_constraintsdialog.h"


/*! \brief Create constraint related triggers.
Sqlite3 does not know how to handle constraints itself.
\author Petr Vanek <petr@scribus.info>
 */
class ConstraintsDialog : public QDialog
{
	Q_OBJECT

	public:
		ConstraintsDialog(const QString & tabName, const QString & schema, QWidget * parent = 0);
		~ConstraintsDialog(){};

		bool update;

	private:
		Ui::ConstraintsDialog ui;
		QString m_schema;
		QString m_table;

		QString createTrigger(const QString & sql);

    private slots:
		/*! \brief Parse user's inputs and create a sql statements */
		void createButton_clicked();
};

#endif
