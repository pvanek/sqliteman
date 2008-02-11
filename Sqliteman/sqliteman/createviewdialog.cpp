/*
For general Sqliteman copyright and licensing information please refer
to the COPYING file provided with the program. Following this notice may exist
a copyright and/or license notice that predates the release of Sqliteman
for which a new license (GPL+exception) is in place.
*/

#include <QPushButton>
#include <QSqlQuery>
#include <QSqlError>

#include "createviewdialog.h"
#include "database.h"


CreateViewDialog::CreateViewDialog(const QString & name, const QString & schema, QWidget * parent)
	: QDialog(parent),
	update(false)
{
	ui.setupUi(this);
	ui.databaseCombo->addItems(Database::getDatabases().keys());

	ui.createButton->setDisabled(true);

	connect(ui.createButton, SIGNAL(clicked()), this, SLOT(createButton_clicked()));
	connect(ui.nameEdit, SIGNAL(textChanged(const QString&)),
			this, SLOT(nameEdit_textChanged(const QString&)));
}

void CreateViewDialog::nameEdit_textChanged(const QString& text)
{
	ui.createButton->setDisabled(text.simplified().isEmpty());
}


void CreateViewDialog::createButton_clicked()
{
	QString sql(QString("CREATE VIEW \"%1\".\"%2\" AS %3;")
			.arg(ui.databaseCombo->currentText())
			.arg(ui.nameEdit->text())
			.arg(ui.sqlEdit->text()));

	QSqlQuery query(sql, QSqlDatabase::database(SESSION_NAME));
	
	if(query.lastError().isValid())
	{
		ui.resultEdit->setText(tr("Error while creating view: %2.\n\n%3").arg(query.lastError().text()).arg(sql));
		return;
	}
	ui.resultEdit->setText(tr("View created successfully"));
	update = true;
	m_schema = ui.databaseCombo->currentText();
	m_name = ui.nameEdit->text();
}
