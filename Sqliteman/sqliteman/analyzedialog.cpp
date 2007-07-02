/*
For general Sqliteman copyright and licensing information please refer
to the COPYING file provided with the program. Following this notice may exist
a copyright and/or license notice that predates the release of Sqliteman
for which a new license (GPL+exception) is in place.
*/

#include "analyzedialog.h"
#include "database.h"


AnalyzeDialog::AnalyzeDialog(QWidget * parent)
	: QDialog(parent)
{
	ui.setupUi(this);

	ui.tableList->addItems(Database::getObjects("table").keys());

	connect(ui.dropButton, SIGNAL(clicked()), this, SLOT(dropButton_clicked()));
	connect(ui.allButton, SIGNAL(clicked()), this, SLOT(allButton_clicked()));
	connect(ui.tableButton, SIGNAL(clicked()), this, SLOT(tableButton_clicked()));
}

void AnalyzeDialog::dropButton_clicked()
{
	Database::execSql("delete from sqlite_stat1;");
}

void AnalyzeDialog::allButton_clicked()
{
	Database::execSql("analyze;");
}

void AnalyzeDialog::tableButton_clicked()
{
	QList<QListWidgetItem *> list(ui.tableList->selectedItems());
	for (int i = 0; i < list.size(); ++i)
	{
		if (!Database::execSql(QString("analyze %1;").arg(list.at(i)->text())))
			break;;
	}
}

