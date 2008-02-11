/*
For general Sqliteman copyright and licensing information please refer
to the COPYING file provided with the program. Following this notice may exist
a copyright and/or license notice that predates the release of Sqliteman
for which a new license (GPL+exception) is in place.
*/

#include <QPushButton>
#include <QSqlQuery>
#include <QSqlError>

#include "constraintsdialog.h"
#include "database.h"


ConstraintsDialog::ConstraintsDialog(const QString & tabName, const QString & schema, QWidget * parent)
	: QDialog(parent),
	m_schema(schema),
	m_table(tabName)
{
	update = false;
	ui.setupUi(this);

	// trigger name templates
	ui.insertName->setText(QString("tr_cons_%1_ins").arg(tabName));
	ui.updateName->setText(QString("tr_cons_%1_upd").arg(tabName));
	ui.deleteName->setText(QString("tr_cons_%1_del").arg(tabName));

	// not nulls
	QStringList inserts;
	QStringList updates;
	QStringList deletes;
	QStringList nnCols;
	QString stmt;
	foreach (DatabaseTableField column, Database::tableFields(tabName, schema))
	{
		if (!column.notnull)
			continue;
		nnCols << column.name;
		stmt = QString("SELECT RAISE(ABORT, 'New %2 value IS NULL') WHERE new.%1 IS NULL;\n")
				.arg(column.name)
				.arg(column.name);
		inserts << "-- NOT NULL check" << stmt;
		updates << "-- NOT NULL check"<< stmt;
	}

	// get FKs
	QString sql(QString("pragma \"%1\".foreign_key_list (\"%2\");").arg(schema).arg(tabName));
	QSqlQuery query(sql, QSqlDatabase::database(SESSION_NAME));
	
	if(query.lastError().isValid())
	{
		ui.resultEdit->setText(tr("Error while parsing constraints: %1.\n\n%2")
				.arg(query.lastError().text())
				.arg(sql));
		return;
	}
	// 2 - table - FK table; 3 - from - column name; 4 - to - fk column name
	QString fkTab;
	QString column;
	QString fkColumn;
	QString nnTemplate;
	QString thenTemplate;
	while (query.next())
	{
		fkTab = query.value(2).toString();
		column = query.value(3).toString();
		fkColumn = query.value(4).toString();
		nnTemplate = "";
		if (nnCols.contains(column, Qt::CaseInsensitive))
		{
			nnTemplate = QString("\n    new.%1 IS NOT NULL AND").arg(column);
		}
		thenTemplate = QString("\n    RAISE(ABORT, '%1 violates foreign key %2(%3)')")
				.arg(column)
				.arg(fkTab)
				.arg(fkColumn);
		stmt = QString("SELECT %1\n    where %2 (SELECT %3 FROM %4 WHERE %5 = new.%6) IS NULL;\n")
				.arg(thenTemplate)
				.arg(nnTemplate)
				.arg(fkColumn)
				.arg(fkTab)
				.arg(fkColumn)
				.arg(column)
				;
		inserts << "-- FK check" << stmt;
		updates << "-- FK check" << stmt;
		deletes << "-- FK check" << QString("SELECT %1 WHERE (SELECT %2 FROM %3 WHERE %4 = old.%5) IS NOT NULL;\n")
				.arg(thenTemplate)
				.arg(fkColumn)
				.arg(fkTab)
				.arg(fkColumn)
				.arg(column);
	}

	// to the GUI
	ui.insertEdit->setText(inserts.join("\n"));
	ui.updateEdit->setText(updates.join("\n"));
	ui.deleteEdit->setText(deletes.join("\n"));

	connect(ui.createButton, SIGNAL(clicked()), this, SLOT(createButton_clicked()));
}

void ConstraintsDialog::createButton_clicked()
{
	QString templ = QString("CREATE TRIGGER %1 BEFORE %2 ON %3.%4 FOR EACH ROW\n"
                       "BEGIN\n"
                       "%5\n\n%6"
                       "END;");

	QString status("INSERT trigger\n");
	createTrigger("begin transaction;");
	if (ui.insertEdit->text().length() != 0)
	{
		status += createTrigger(templ.arg(ui.insertName->text())
								.arg("INSERT")
								.arg(m_schema)
								.arg(m_table)
								.arg("-- created by Sqliteman tool")
								.arg(ui.insertEdit->text()));
	}
	else
		status += tr("No action for INSERT");

	status += "\nUPDATE trigger\n";
	if (ui.updateEdit->text().length() != 0)
	{
		status += createTrigger(templ.arg(ui.updateName->text())
								.arg("UPDATE")
								.arg(m_schema)
								.arg(m_table)
								.arg("-- created by Sqliteman tool")
								.arg(ui.updateEdit->text()));
	}
	else
		status += tr("No action for UPDATE");

	status += "\nDELETE trigger\n";
	if (ui.deleteEdit->text().length() != 0)
	{
		status += createTrigger(templ.arg(ui.deleteName->text())
								.arg("DELETE")
								.arg(m_schema)
								.arg(m_table)
								.arg("-- created by Sqliteman tool")
								.arg(ui.deleteEdit->text()));
	}
	else
		status += tr("No action for DELETE");
	createTrigger("commit;");
	ui.resultEdit->setText(status);
}

QString ConstraintsDialog::createTrigger(const QString & sql)
{
	QString ret;
	QSqlQuery query(sql, QSqlDatabase::database(SESSION_NAME));
	if(query.lastError().isValid())
		return tr("Error while creating trigger: %1.").arg(query.lastError().text());
	update = true;
	return tr("Trigger created successfully");
}
