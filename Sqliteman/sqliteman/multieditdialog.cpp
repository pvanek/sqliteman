/*
For general Sqliteman copyright and licensing information please refer
to the COPYING file provided with the program. Following this notice may exist
a copyright and/or license notice that predates the release of Sqliteman
for which a new license (GPL+exception) is in place.
*/
#include <QFileInfo>
#include <QFileDialog>
#include <QMessageBox>

#include "multieditdialog.h"
#include "preferences.h"


MultiEditDialog::MultiEditDialog(QWidget * parent)
	: QDialog(parent)
{
	setupUi(this);

	connect(blobFileEdit, SIGNAL(textChanged(const QString &)),
			this, SLOT(blobFileEdit_textChanged(const QString &)));
	connect(tabWidget, SIGNAL(currentChanged(int)),
			this, SLOT(tabWidget_currentChanged(int)));
	connect(blobFileButton, SIGNAL(clicked()),
			this, SLOT(blobFileButton_clicked()));
	connect(blobSaveButton, SIGNAL(clicked()),
			this, SLOT(blobSaveButton_clicked()));
	connect(nullCheckBox, SIGNAL(stateChanged(int)),
			this, SLOT(nullCheckBox_stateChanged(int)));
}

void MultiEditDialog::setData(const QVariant & data)
{
	m_data = data;
	textEdit->setPlainText(data.toString());
	dateFormatEdit->setText(Preferences::instance()->dateTimeFormat());
	dateTimeEdit->setDate(QDateTime::currentDateTime().date());
	blobPreviewLabel->setBlobData(data);

	// Prevent possible text related modification of BLOBs.
	// It can be done in text editor.
	if (data.type() == QVariant::ByteArray)
		tabWidget->setCurrentIndex(1);
	else
		tabWidget->setCurrentIndex(0);
}

QVariant MultiEditDialog::data()
{
	QVariant ret;
	// NULL
	if (nullCheckBox->isChecked())
		return QVariant(QString());

	switch (tabWidget->currentIndex())
	{
		// handle text with EOLs
		case 0:
			ret = textEdit->toPlainText();
			break;
		// handle File2BLOB
		case 1:
		{
			QFile f(blobFileEdit->text());
			if (f.open(QIODevice::ReadOnly))
				ret = QVariant(f.readAll())/*.data()*/;
			break;
		}
		// handle DateTime to string
		case 2:
			Preferences::instance()->setDateTimeFormat(dateFormatEdit->text());
			ret =  dateTimeEdit->dateTime().toString(dateFormatEdit->text());
			break;
	}
	return ret;
}

void MultiEditDialog::blobFileButton_clicked()
{
	QString fileName = QFileDialog::getOpenFileName(this,
													tr("Open File"),
													blobFileEdit->text(),
													tr("All Files (* *.*)"));
	if (!fileName.isNull())
	{
		blobFileEdit->setText(fileName);
		blobPreviewLabel->setBlobFromFile(fileName);
	}
}

void MultiEditDialog::blobSaveButton_clicked()
{
	QString fileName = QFileDialog::getSaveFileName(this,
													tr("Open File"),
			   										blobFileEdit->text(),
													tr("All Files (* *.*)"));
	if (fileName.isNull())
		return;
	QFile f(fileName);
	if (!f.open(QIODevice::WriteOnly))
	{
		QMessageBox::warning(this, tr("BLOB Save Error"),
							 tr("Cannot open file %1 for writting").arg(fileName));
		return;
	}
	if (f.write(m_data.toByteArray()) == -1)
	{
		QMessageBox::warning(this, tr("BLOB Save Error"),
							 tr("Cannot write into file %1").arg(fileName));
		return;
	}
	f.close();
}

void MultiEditDialog::blobFileEdit_textChanged(const QString &)
{
	checkButtonStatus();
}

void MultiEditDialog::tabWidget_currentChanged(int)
{
	checkButtonStatus();
}

void MultiEditDialog::checkButtonStatus()
{
	bool e = true;
	// NULL
	if (nullCheckBox->isChecked())
		e = true;
	else
	{
		switch (tabWidget->currentIndex())
		{
			case 0:
				break;
			case 1:
			{
				QString text(blobFileEdit->text().simplified());
				if (text.isNull() || text.isEmpty() || !QFileInfo(text).isFile())
					e = false;
				break;
			}
			case 2:
				break;
		}
	}
	buttonBox->button(QDialogButtonBox::Ok)->setEnabled(e);
}

// void MultiEditDialog::checkBlobPreview(QVariant data)
// {
// 	QPixmap pm;
// 	pm.loadFromData(data.toByteArray());
// 	if (pm.isNull())
// 		blobPreviewLabel->setText(tr("File content cannot be displayed"));
// 	else
// 		blobPreviewLabel->setPixmap(pm.scaled(blobPreviewLabel->size(), Qt::KeepAspectRatio));
// }

// void MultiEditDialog::checkBlobPreview(const QString & fileName)
// {
// 	QPixmap pm(fileName);
// 	if (pm.isNull())
// 		blobPreviewLabel->setText(tr("File content cannot be displayed"));
// 	else
// 		blobPreviewLabel->setPixmap(pm.scaled(blobPreviewLabel->size(), Qt::KeepAspectRatio));
// }

void MultiEditDialog::nullCheckBox_stateChanged(int)
{
	tabWidget->setDisabled(nullCheckBox->isChecked());
	checkButtonStatus();
}
