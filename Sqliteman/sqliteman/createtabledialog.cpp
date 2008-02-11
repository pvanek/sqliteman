/*
For general Sqliteman copyright and licensing information please refer
to the COPYING file provided with the program. Following this notice may exist
a copyright and/or license notice that predates the release of Sqliteman
for which a new license (GPL+exception) is in place.
*/

#include <QCheckBox>
#include <QSqlQuery>
#include <QSqlError>
#include <QMessageBox>

#include "createtabledialog.h"
#include "database.h"


CreateTableDialog::CreateTableDialog(QWidget * parent)
	: TableEditorDialog(parent)
{
	update = false;
	ui.removeButton->setEnabled(false); // Disable row removal
	addField(); // A table should have at least one field
	setWindowTitle(tr("Create Table"));

	ui.createButton->setDisabled(true);

	ui.textEdit->setText("CREATE TABLE [IF NOT EXISTS] <database-name.table-name>\n\
(\n\
    <column-name> <type> <constraint...>,\n\
    ...\n\
    [, constraints ]\n\
)");
}

QString CreateTableDialog::getSQLfromGUI()
{
	QString sql(QString("CREATE TABLE %1 (\n").arg(getFullName(ui.nameEdit->text())));
	QString nn;
	QString def;
	DatabaseTableField f;

	for(int i = 0; i < ui.columnTable->rowCount(); i++)
	{
		f = getColumn(i);
		sql += getColumnClause(f);
	}
	sql = sql.remove(sql.size() - 2, 2); 	// cut the extra ", "
	sql += "\n);\n";

	return sql;
}

void CreateTableDialog::createButton_clicked()
{
	QString sql;
	// from GUI
	if (ui.tabWidget->currentIndex() == 0)
		sql = getSQLfromGUI();
	else
		sql = ui.textEdit->text();

	QSqlQuery query(sql, QSqlDatabase::database(SESSION_NAME));
	if(query.lastError().isValid())
	{
		ui.resultEdit->setText(tr("Error while creating table: %1.\n\n%2").arg(query.lastError().text()).arg(sql));
		return;
	}
	update = true;
	ui.resultEdit->setText(tr("Table created successfully"));
}

void CreateTableDialog::tabWidget_currentChanged(int index)
{
	if (index == 1)
	{
		int com = QMessageBox::question(this, tr("Sqliteman"),
						tr("The current content of the Advanced SQL editor will be lost."
						   "Do you really want to recreate your SQL?"),
						   QMessageBox::Yes, QMessageBox::No, QMessageBox::Cancel);
		if (com == QMessageBox::Yes)
			ui.textEdit->setText(getSQLfromGUI());
		else if (com == QMessageBox::Cancel)
			ui.tabWidget->setCurrentIndex(0);
	}
	TableEditorDialog::tabWidget_currentChanged(index);
}
