/*
For general Sqliteman copyright and licensing information please refer
to the COPYING file provided with the program. Following this notice may exist
a copyright and/or license notice that predates the release of Sqliteman
for which a new license (GPL+exception) is in place.
*/

#ifndef IMPORTTABLELOGDIALOG_H
#define IMPORTTABLELOGDIALOG_H

#include "ui_importtablelogdialog.h"

#include <QDialog>


/*! \brief Simple import log viewer.
Used from ImportTableDialog as a result viewer when an import error occurs.
\author Petr Vanek <petr@scribus.info>
*/
class ImportTableLogDialog : public QDialog, public Ui::ImportTableLogDialog
{
	Q_OBJECT

	public:
		ImportTableLogDialog(QStringList log, QWidget * parent = 0);
};

#endif
