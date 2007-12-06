/*
For general Sqliteman copyright and licensing information please refer
to the COPYING file provided with the program. Following this notice may exist
a copyright and/or license notice that predates the release of Sqliteman
for which a new license (GPL+exception) is in place.
*/
#ifndef POPULATORDIALOG_H
#define POPULATORDIALOG_H

#include "ui_populatordialog.h"
#include "populatorstructs.h"
#include "database.h"


/*! \brief Simple (testing/QA) data generator for tables.
Populator tries to guess what to insert into the table. It
finds if the requested value is number/text/primary key and
its size.
When user choose "resume errors" checkbox in its GUI the Populator
runs until the end and does not check any errors. When it's disabled
by user, the first error (e.g. unique constraint or trigger test
violated) stops the execution.

Currently implemented actions:
T_AUTO: it populates values with max(column)+1 number values
T_NUMB: random number for column size
T_TEXT: random text for column size
T_PREF: prefixed text. See textPrefixedValues() for more info.
T_STAT: static value. No computings, only user given string/number.
T_IGNORE: nothing inserted. It's left for table default/null value.

\author Petr Vanek <petr@scribus.ifno>
*/
class PopulatorDialog : public QDialog, public Ui::PopulatorDialog
{
	Q_OBJECT

	public:
		PopulatorDialog(QWidget * parent = 0, const QString & table = 0, const QString & schema = 0);
		
	private:
		QString m_schema;
		QString m_table;

		QList<Populator::PopColumn> m_columnList;

		//! Generate the column part of SQL statement
		QString sqlColumns();
		//! Generate the bind part of SQL statement
		QString sqlBinds();

		//! Create PK values (max()+1)
		QVariantList autoValues(Populator::PopColumn c);
		//! Calculate pseudo-random numbers for given column
		QVariantList numberValues(Populator::PopColumn c);
		//! Create a text for given column
		QVariantList textValues(Populator::PopColumn c);
		/*! \brief Create a text with prefix for given column.
		The prefix is specified by user in the PopulatorColumnWidget
		instance if the c.action is T_PREF. Values are returned as:
		${prefix}1, ${prefix}2, ..., ${prefix}N */
		QVariantList textPrefixedValues(Populator::PopColumn c);
		//! User given value only.
		QVariantList staticValues(Populator::PopColumn c);

		/*! Performs select count from table to simulate
		QSqlQuery::numRowsAffected() for execBatch() method (it returns
		1/0/-1 in the case of execBatch()). */
		qlonglong tableRowCount();

	private slots:
		void populateButton_clicked();
		void spinBox_valueChanged(int);
		//! Set populateButton state (enabled/disabled) as required.
		void checkActionTypes();
};

#endif
