/*
For general Sqliteman copyright and licensing information please refer
to the COPYING file provided with the program. Following this notice may exist
a copyright and/or license notice that predates the release of Sqliteman
for which a new license (GPL+exception) is in place.
*/
#ifndef MULTIEDITDIALOG_H
#define MULTIEDITDIALOG_H

#include "ui_multieditdialog.h"


/*! \brief Enthanced modal editor for custom delegate.
User handles here large texts (more than 1 line), files to BLOBs, and date strings.
DateTime mask/format can be setup as in Qt4 classes:
http://doc.trolltech.com/4.3/qdatetime.html#toString
\author Petr Vanek <petr@scribus.info>
*/
class MultiEditDialog : public QDialog, public Ui::MultiEditDialog
{
	Q_OBJECT

	public:
		MultiEditDialog(QWidget * parent = 0);
		
		void setData(const QVariant & data);
		QVariant data();

	private:
		QVariant m_data;

		void checkButtonStatus();
// 		void checkBlobPreview(QVariant data);
// 		void checkBlobPreview(const QString & fileName);

	private slots:
		void blobFileEdit_textChanged(const QString &);
		void tabWidget_currentChanged(int);
		void blobFileButton_clicked();
		void blobSaveButton_clicked();
		void nullCheckBox_stateChanged(int);
};

#endif
