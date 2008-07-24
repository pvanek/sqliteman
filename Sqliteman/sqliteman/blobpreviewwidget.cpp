/*
For general Sqliteman copyright and licensing information please refer
to the COPYING file provided with the program. Following this notice may exist
a copyright and/or license notice that predates the release of Sqliteman
for which a new license (GPL+exception) is in place.
*/
#include <QVariant>
#include <QFile>

#include "blobpreviewwidget.h"


BlobPreviewWidget::BlobPreviewWidget(QWidget * parent)
	: QWidget(parent)
{
	setupUi(this);
}

void BlobPreviewWidget::setBlobData(QVariant data)
{
	m_data = data.toByteArray();
	createPreview();
}

void BlobPreviewWidget::createPreview()
{
	QPixmap pm;
	pm.loadFromData(m_data);

	if (pm.isNull())
		m_blobPreview->setText("<qt>" + tr("File content cannot be displayed") + "</qt>");
	else
	{
		// HACK: "-3" constant are there to prevent recursive
		// growing in Qt events.
		if (pm.width() > m_blobPreview->width() - 3
				  || pm.height() > m_blobPreview->height() - 3)
		{
			QSize sz(m_blobPreview->size().width()-3, m_blobPreview->size().height()-3);
			m_blobPreview->setPixmap(pm.scaled(sz, Qt::KeepAspectRatio));
		}
		else
			m_blobPreview->setPixmap(pm);
	}
	m_blobSize->setText(formatSize(m_data.size()));
}

void BlobPreviewWidget::setBlobFromFile(const QString & fileName)
{
	QFile file(fileName);
	if (file.open(QIODevice::ReadOnly))
	{
		m_data = file.readAll();
	}
	else
		m_data = QByteArray();
	createPreview();
}

void BlobPreviewWidget::resizeEvent(QResizeEvent * event)
{
	createPreview();
	QWidget::resizeEvent(event);
}

QString BlobPreviewWidget::formatSize(qulonglong size/*, bool persec*/)
{
	QString rval;

	if(size < 1024)
		rval = QString("%L1 B").arg(size);
	else if(size < 1024*1024)
		rval = QString("%L1 KB").arg(size/1024);
	else if(size < 1024*1024*1024)
		rval = QString("%L1 MB").arg(double(size)/1024.0/1024.0, 0, 'f', 1);
	else
		rval = QString("%L1 GB").arg(double(size)/1024.0/1024.0/1024.0, 0, 'f', 1);

// 	if(persec) rval += "/s";
	return rval;
}
