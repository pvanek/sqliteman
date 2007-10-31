/*
For general Sqliteman copyright and licensing information please refer
to the COPYING file provided with the program. Following this notice may exist
a copyright and/or license notice that predates the release of Sqliteman
for which a new license (GPL+exception) is in place.
*/
#ifndef MULTIEDITDIALOG_H
#define MULTIEDITDIALOG_H

#include "ui_multieditdialog.h"


class MultiEditDialog : public QDialog, public Ui::MultiEditDialog
{
	Q_OBJECT

	public:
		MultiEditDialog(QWidget * parent = 0);
		
		void setData(const QString & data);
		QString data();

	private:
		void checkButtonStatus();
		void checkBlobPreview(uchar * data);
		void checkBlobPreview(const QString & fileName);

	private slots:
		void blobFileEdit_textChanged(const QString &);
		void tabWidget_currentChanged(int);
		void blobFileButton_clicked();
};

#endif
