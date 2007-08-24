/*
For general Sqliteman copyright and licensing information please refer
to the COPYING file provided with the program. Following this notice may exist
a copyright and/or license notice that predates the release of Sqliteman
for which a new license (GPL+exception) is in place.
*/
#include <QFileDialog>

#include "importtabledialog.h"
#include "database.h"
#include "sqliteprocess.h"

#include <QtDebug>
ImportTableDialog::ImportTableDialog(QWidget * parent, const QString & tableName, const QString & schema)
	: QDialog(parent),
	  m_parent(parent)
{
	setupUi(this);

	QString n;
	int i = 0;
	int currIx = 0;
	foreach (n, Database::getObjects("table", schema).keys())
	{
		if (n == tableName)
			currIx = i;
		tableComboBox->addItem(n);
		++i;
	}
	tableComboBox->setCurrentIndex(currIx);

	connect(fileButton, SIGNAL(clicked()), this, SLOT(fileButton_clicked()));
	connect(buttonBox, SIGNAL(accepted()), this, SLOT(slotAccepted()));
}

void ImportTableDialog::fileButton_clicked()
{
	QString pth(fileEdit->text());
	pth = pth.isEmpty() ? QDir::currentPath() : pth;
	QString fname = QFileDialog::getOpenFileName(this, tr("File to Import"),
												 pth,
												 tr("Text Files (*.txt);;All Files (*)"));
	if (fname.isEmpty())
		return;

	fileEdit->setText(fname);
}

void ImportTableDialog::slotAccepted()
{
	bool result = false;
	switch (tabWidget->currentIndex())
	{
		case 0 :
			result = sqliteImport();
			break;
		// TODO: more stuff!
	}

	if (result)
		accept();
}

bool ImportTableDialog::sqliteImport()
{
	QString out, sep;
	SqliteProcess imp(m_parent);

	if (pipeRadioButton->isChecked())
		sep = "|";
	else if (commaRadioButton->isChecked())
		sep = ",";
	else if (semicolonRadioButton->isChecked())
		sep = ";";
	else if (tabelatorRadioButton->isChecked())
		sep = "\"\\t\"";
	else if (customRadioButton->isChecked())
		sep = customEdit->text();

	imp.start(QStringList() << ".separator " << sep << " .import " << fileEdit->text() << " " << tableComboBox->currentText());
	qDebug() << imp.errorMessage();
	qDebug() << imp.success();
	qDebug() << imp.allStderr();
	return false;
}
