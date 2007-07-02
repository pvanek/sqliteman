/*
For general Sqliteman copyright and licensing information please refer
to the COPYING file provided with the program. Following this notice may exist
a copyright and/or license notice that predates the release of Sqliteman
for which a new license (GPL+exception) is in place.
*/

#ifndef ALTERTRIGGERDIALOG_H
#define ALTERTRIGGERDIALOG_H

#include <qwidget.h>

#include "ui_createtriggerdialog.h"


/*! \brief GUI for trigger altering
\author Petr Vanek <petr@scribus.info>
*/
class AlterTriggerDialog : public QDialog
{
	Q_OBJECT

	public:
		AlterTriggerDialog(const QString & name, const QString & schema, QWidget * parent = 0);
		~AlterTriggerDialog(){};

// 		bool update;

	private:
		Ui::CreateTriggerDialog ui;
		QString m_schema;
		QString m_name;

	private slots:
		void createButton_clicked();
};

#endif
