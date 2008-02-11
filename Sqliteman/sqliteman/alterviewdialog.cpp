/*
For general Sqliteman copyright and licensing information please refer
to the COPYING file provided with the program. Following this notice may exist
a copyright and/or license notice that predates the release of Sqliteman
for which a new license (GPL+exception) is in place.
*/

#include <QPushButton>
#include <QSqlQuery>
#include <QSqlError>
#include <QtDebug>

#include "alterviewdialog.h"
#include "database.h"


AlterViewDialog::AlterViewDialog(const QString & name, const QString & schema, QWidget * parent)
	: QDialog(parent),
	update(false)
{
	ui.setupUi(this);
	ui.databaseCombo->addItem(schema);
	ui.nameEdit->setText(name);
	ui.databaseCombo->setDisabled(true);
	ui.nameEdit->setDisabled(true);

	QSqlQuery query(QString("select sql from \"%1\".sqlite_master where name = '%2';").arg(schema).arg(name),
					 QSqlDatabase::database(SESSION_NAME));
	while (query.next())
	{
		QString s(query.value(0).toString());
// 		int pos = s.indexOf(QRegExp("(\\b|\\W)AS(\\b|\\W)",  Qt::CaseInsensitive));
		int pos = s.indexOf(QRegExp("\\bAS\\b",  Qt::CaseInsensitive));
		if (pos == -1)
			qDebug() << "regexp parse error. Never should be written out";
		else
			ui.sqlEdit->setText(s.right(s.length() - pos - 2).trimmed());
	}

	setWindowTitle(tr("Alter View"));
	ui.createButton->setText("&Alter");

	connect(ui.createButton, SIGNAL(clicked()), this, SLOT(createButton_clicked()));
}

void AlterViewDialog::createButton_clicked()
{
	update = true;
	ui.resultEdit->clear();
	QString sql(QString("DROP VIEW \"%1\".\"%2\"")
			.arg(ui.databaseCombo->currentText())
			.arg(ui.nameEdit->text()));
	QSqlQuery dropQuery(sql, QSqlDatabase::database(SESSION_NAME));
	if (dropQuery.lastError().isValid())
	{
		ui.resultEdit->insertPlainText(tr("Error while altering view (drop phase): %1.\n\n%2").arg(dropQuery.lastError().text()).arg(sql));
		ui.resultEdit->moveCursor(QTextCursor::Start);
	}

	sql = QString("CREATE VIEW \"%1\".\"%2\" AS\n%3;")
			.arg(ui.databaseCombo->currentText())
			.arg(ui.nameEdit->text())
			.arg(ui.sqlEdit->text());
	QSqlQuery query(sql, QSqlDatabase::database(SESSION_NAME));
	
	if(query.lastError().isValid())
	{
		ui.resultEdit->insertPlainText(tr("Error while altering view: %1.\n\n%2").arg(query.lastError().text()).arg(sql));
		ui.resultEdit->insertPlainText("\n");
		return;
	}
	ui.resultEdit->insertPlainText(tr("View altered successfully"));
	ui.resultEdit->insertPlainText("\n");
}
