/*
For general Sqliteman copyright and licensing information please refer
to the COPYING file provided with the program. Following this notice may exist
a copyright and/or license notice that predates the release of Sqliteman
for which a new license (GPL+exception) is in place.
*/

#ifndef CREATEVIEWDIALOG_H
#define CREATEVIEWDIALOG_H

#include <qwidget.h>

#include "ui_createviewdialog.h"


/*! \brief GUI for view creation
\author Petr Vanek <petr@scribus.info>
*/
class CreateViewDialog : public QDialog
{
	Q_OBJECT

	public:
		CreateViewDialog(const QString & name, const QString & schema, QWidget * parent = 0);
		~CreateViewDialog(){};

		bool update;
		void setText(const QString & text) { ui.sqlEdit->setText(text); };

		QString schema() { return m_schema; };
		QString name() { return m_name; };

	private:
		Ui::CreateViewDialog ui;

		QString m_schema;
		QString m_name;

	private slots:
		void createButton_clicked();
		void nameEdit_textChanged(const QString& text);
};

#endif
