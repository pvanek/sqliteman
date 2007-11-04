/*
For general Sqliteman copyright and licensing information please refer
to the COPYING file provided with the program. Following this notice may exist
a copyright and/or license notice that predates the release of Sqliteman
for which a new license (GPL+exception) is in place.
*/

#include <QIcon>

#include "utils.h"


QIcon getIcon(const QString & fileName)
{
	return QIcon(QString(ICON_DIR) + "/" + fileName);
}

QString getTranslator(const QString & localeName)
{
	return QString("%1/sqliteman_%2.qm").arg(TRANSLATION_DIR).arg(localeName);
}
