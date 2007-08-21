/*
For general Sqliteman copyright and licensing information please refer
to the COPYING file provided with the program. Following this notice may exist
a copyright and/or license notice that predates the release of Sqliteman
for which a new license (GPL+exception) is in place.
*/
#include <QMessageBox>
#include "shortcuteditordialog.h"
#include "shortcutmodel.h"


ShortcutEditorDialog::ShortcutEditorDialog(QWidget * parent)
	: QDialog(parent)
{
	setupUi(this);

	model = new ShortcutModel();
	tableView->setModel(model);
	tableView->resizeColumnsToContents();

	connect(removeAllButton, SIGNAL(clicked()), this, SLOT(removeAllButton_clicked()));
	connect(removeButton, SIGNAL(clicked()), this, SLOT(removeButton_clicked()));
	connect(addButton, SIGNAL(clicked()), this, SLOT(addButton_clicked()));
	connect(model, SIGNAL(keysNotUnique(QString)), this, SLOT(keysNotUnique(QString)));
	connect(buttonBox, SIGNAL(accepted()), this, SLOT(acceptDialog()));
}

ShortcutEditorDialog::~ShortcutEditorDialog()
{
}

void ShortcutEditorDialog::removeAllButton_clicked()
{
	model->removeRows(0, model->rowCount());
}

void ShortcutEditorDialog::removeButton_clicked()
{
	model->removeRows(tableView->currentIndex().row(), 1);
}

void ShortcutEditorDialog::addButton_clicked()
{
	model->insertRow();
}

void ShortcutEditorDialog::keysNotUnique(QString value)
{
	QMessageBox::warning(this, tr("Shortcut Error"),
						 tr("The value you entered as a key (%1) is not unique. Enter another one, please.").arg(value));
}

void ShortcutEditorDialog::acceptDialog()
{
	QPair<QString,QString> p;
	foreach (p, model->values())
	{
		if (p.first.isEmpty())
		{
			int b = QMessageBox::question(this, tr("Shortcut Error"),
										  tr("Some of key values are empty. These items will be lost. Do you want to return to the shortcut editor to fix it?"),
										  QMessageBox::Yes | QMessageBox::No,
										  QMessageBox::Yes);
			if (b == QMessageBox::Yes)
				return;
		}
	}
	model->saveValues();
	accept();
}
