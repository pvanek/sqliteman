/*
For general Sqliteman copyright and licensing information please refer
to the COPYING file provided with the program. Following this notice may exist
a copyright and/or license notice that predates the release of Sqliteman
for which a new license (GPL+exception) is in place.
*/
#include <QFileInfo>
#include <QFileDialog>

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
}

void MultiEditDialog::setData(const QString & data)
{
	textEdit->setPlainText(data);
	dateFormatEdit->setText(Preferences::instance()->dateTimeFormat());
	dateTimeEdit->setDate(QDateTime::currentDateTime().date());
}

QString MultiEditDialog::data()
{
	QString ret;
	switch (tabWidget->currentIndex())
	{
		// handle text with EOLs
		case 0:
			ret = textEdit->toPlainText();
			break;
		// handle File2BLOB
		case 1:
			break;
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
		checkBlobPreview(fileName);
	}
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
	buttonBox->button(QDialogButtonBox::Ok)->setEnabled(e);
}

void MultiEditDialog::checkBlobPreview(uchar * data)
{
}

void MultiEditDialog::checkBlobPreview(const QString & fileName)
{
	QPixmap pm(fileName);
	if (pm.isNull())
		blobPreviewLabel->setText(tr("File content cannot be displayed"));
	else
		blobPreviewLabel->setPixmap(pm.scaled(blobPreviewLabel->size(), Qt::KeepAspectRatio));
}
