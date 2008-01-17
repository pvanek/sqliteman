/*
For general Sqliteman copyright and licensing information please refer
to the COPYING file provided with the program. Following this notice may exist
a copyright and/or license notice that predates the release of Sqliteman
for which a new license (GPL+exception) is in place.
*/

#ifndef TABLEEDITORDIALOG_H
#define TABLEEDITORDIALOG_H

#include <QDialog>

#include "database.h"
#include "ui_tableeditordialog.h"


/*! \brief A base dialog for creating and editing tables.
This dialog is taken as a inheritance parent for AlterTableDialog
and CreateTableDialog.
\author Petr Vanek <petr@scribus.info>
\author Igor Khanin
*/
class TableEditorDialog : public QDialog
{
		Q_OBJECT
	public:
		TableEditorDialog(QWidget * parent);
		~TableEditorDialog();

		Ui::TableEditorDialog ui;

		QString getFullName(const QString & objName);

		DatabaseTableField getColumn(int row);

		/*! \brief Gues what default value should be set.
		If there is a successful conversion to the number,
		default(foo) is used = no change for string.
		If there is a "'" at the first place - default(foo)
		is used = no change for string.
		Else use default('foo') = add SQL string mark.
		*/
		QString getDefaultClause(const QString & defVal);
		
		QString getColumnClause(DatabaseTableField column);

	public slots:
		virtual void addField();
		virtual void removeField();
		virtual void fieldSelected();
		virtual void nameEdit_textChanged(const QString&);
		virtual void createButton_clicked();
		virtual void tabWidget_currentChanged(int index);

};

#endif
