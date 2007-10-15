/*
For general Sqliteman copyright and licensing information please refer
to the COPYING file provided with the program. Following this notice may exist
a copyright and/or license notice that predates the release of Sqliteman
for which a new license (GPL+exception) is in place.
*/

#include "vacuumdialog.h"
#include "database.h"


VacuumDialog::VacuumDialog(QWidget * parent)
	: QDialog(parent)
{
	ui.setupUi(this);

	ui.tableList->addItems(Database::getObjects("table").values());
	ui.tableList->addItems(Database::getObjects("index").values());

	connect(ui.allButton, SIGNAL(clicked()), this, SLOT(allButton_clicked()));
	connect(ui.tableButton, SIGNAL(clicked()), this, SLOT(tableButton_clicked()));
}

void VacuumDialog::allButton_clicked()
{
	Database::execSql("vacuum;");
}

void VacuumDialog::tableButton_clicked()
{
	QList<QListWidgetItem *> list(ui.tableList->selectedItems());
	for (int i = 0; i < list.size(); ++i)
	{
		if (!Database::execSql(QString("vacuum %1;").arg(list.at(i)->text())))
			break;
	}
}

