/*
For general Sqliteman copyright and licensing information please refer
to the COPYING file provided with the program. Following this notice may exist
a copyright and/or license notice that predates the release of Sqliteman
for which a new license (GPL+exception) is in place.
*/
#include <QPushButton>
#include <QSqlQuery>
#include <QSqlError>

#include "altertriggerdialog.h"
#include "database.h"


AlterTriggerDialog::AlterTriggerDialog(const QString & name, const QString & schema, QWidget * parent)
	: QDialog(parent),
// 	update(false),
	m_schema(schema),
	m_name(name)
{
	ui.setupUi(this);
	ui.createButton->setText(tr("&Alter"));
	setWindowTitle("Alter Trigger");

	QString sql(QString("select sql from \"%1\".sqlite_master where name = '%2';").arg(schema).arg(name));
	QSqlQuery query(sql, QSqlDatabase::database(SESSION_NAME));
	if (query.lastError().isValid())
		ui.textEdit->setText(tr("Cannot get trigger from the database."));
	else
	{
		while (query.next())
		{
			ui.textEdit->setText(query.value(0).toString());
			break;
		}
	}

	connect(ui.createButton, SIGNAL(clicked()), this, SLOT(createButton_clicked()));
}


void AlterTriggerDialog::createButton_clicked()
{
	QSqlQuery drop(QString("DROP TRIGGER \"%1\".\"%2\";").arg(m_schema).arg(m_name),
				  QSqlDatabase::database(SESSION_NAME));
	if(drop.lastError().isValid())
	{
		ui.resultEdit->setText(tr("Cannot drop trigger: %1.\n\n%2").arg(drop.lastError().text()).arg(m_name));
		return;
	}

	QString sql(ui.textEdit->text());
	QSqlQuery query(sql, QSqlDatabase::database(SESSION_NAME));
	
	if(query.lastError().isValid())
	{
		ui.resultEdit->setText(tr("Error while creating trigger: %2.\n\n%3").arg(query.lastError().text()).arg(sql));
		return;
	}
	ui.resultEdit->setText(tr("Trigger created successfully"));
// 	update = true;
}
