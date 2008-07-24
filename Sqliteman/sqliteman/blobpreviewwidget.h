/*
For general Sqliteman copyright and licensing information please refer
to the COPYING file provided with the program. Following this notice may exist
a copyright and/or license notice that predates the release of Sqliteman
for which a new license (GPL+exception) is in place.
*/

#ifndef BLOBPREVIEWDIALOG_H
#define BLOBPREVIEWDIALOG_H

#include "ui_blobpreviewwidget.h"


/*! \brief Brute force BLOB to Image converter.
Methods setBlobData() and setBlobFromFile() try convert BLOBs into images
supported by Qt4 to create a image previews.
It displays data size for all values.
*/
class BlobPreviewWidget : public QWidget, public Ui::BlobPreviewWidget
{
	Q_OBJECT
	
	public:
		BlobPreviewWidget(QWidget * parent = 0);
		void setBlobData(QVariant data);
		void setBlobFromFile(const QString & fileName);

	private:
		QByteArray m_data;

		void resizeEvent(QResizeEvent * event);
		void createPreview();

		/*! \brief Format m_data size to the human readable form.
		It's taken from FatRat http://fatrat.dolezel.info/. Cheers!
		*/
		QString formatSize(qulonglong size/*, bool persec*/);
};

#endif
