/*
For general Sqliteman copyright and licensing information please refer
to the COPYING file provided with the program. Following this notice may exist
a copyright and/or license notice that predates the release of Sqliteman
for which a new license (GPL+exception) is in place.
*/
#ifndef POPULATORDIALOG_H
#define POPULATORDIALOG_H

#include "ui_populatordialog.h"
#include "database.h"


/*! \brief Simple (testing/QA) data generator for tables.
Populator tries to guess what to insert into the table. It
finds if the requested value is number/text/primary key and
its size.
When user choose "resume errors" checkbox in its GUI the Populator
runs until the end and does not check any errors. When it's disabled
by user, the first error (e.g. unique constraint or trigger test
violated) stops the execution.
*/
class PopulatorDialog : public QDialog, public Ui::PopulatorDialog
{
	Q_OBJECT

	public:
		PopulatorDialog(QWidget * parent = 0, const QString & table = 0, const QString & schema = 0);
		
	private:
		QString m_schema;
		QString m_table;

		typedef struct
		{
			QString name;
			QString type;
			bool pk;
			int action;
// 			int autoCounter;
			int size;
		}
		PopColumn;
		QList<PopColumn> columnList;
		QMap<int,QString> actionMap;

		//! Guess what it can insert as value
		int defaultSuggestion(const DatabaseTableField & column);
		//! Generate the column part of SQL statement
		QString sqlColumns();
		//! Generate the bind part of SQL statement
		QString sqlBinds();

		//! Create PK values (max()+1)
		QVariantList autoValues(PopColumn c);
		//! Calculate pseudo-random numbers for given column
		QVariantList numberValues(PopColumn c);
		//! Create a text for given column
		QVariantList textValues(PopColumn c);

	private slots:
		void populateButton_clicked();
};

#endif
