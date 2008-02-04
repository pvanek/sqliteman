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
	: QLabel(parent)
{
}

void BlobPreviewWidget::setBlobData(QVariant data)
{
	m_data = data.toByteArray();
	createPreview(m_data);
}

void BlobPreviewWidget::createPreview(QByteArray data)
{
	QPixmap pm;
	pm.loadFromData(data);
	if (pm.isNull())
		setText(tr("File content cannot be displayed"));
	else
	{
		if (pm.width() > width() || pm.height() > height())
			setPixmap(pm.scaled(size(), Qt::KeepAspectRatio));
		else
			setPixmap(pm);
	}
}

void BlobPreviewWidget::setBlobFromFile(const QString & fileName)
{
	QByteArray pm;
	QFile file(fileName);
	if (file.open(QIODevice::ReadOnly))
	{
		pm = file.readAll();
	}
	createPreview(pm);
}

void BlobPreviewWidget::resizeEvent(QResizeEvent * event)
{
	createPreview(m_data);
	QLabel::resizeEvent(event);
}
