/*
For general Sqliteman copyright and licensing information please refer
to the COPYING file provided with the program. Following this notice may exist
a copyright and/or license notice that predates the release of Sqliteman
for which a new license (GPL+exception) is in place.
*/
#ifndef POPULATORDIALOG_H
#define POPULATORDIALOG_H

#include "ui_populatordialog.h"
#include "database.h"


class PopulatorDialog : public QDialog, public Ui::PopulatorDialog
{
	Q_OBJECT

	public:
		PopulatorDialog(QWidget * parent = 0, const QString & table = 0, const QString & schema = 0);
		
	private:
		QString m_schema;
		QString m_table;

		typedef struct
		{
			QString name;
			QString type;
			bool pk;
			int action;
			int autoCounter;
			int size;
		}
		PopColumn;
		QList<PopColumn> columnList;
		QMap<int,QString> actionMap;
		
		int defaultSuggestion(const DatabaseTableField & column);
		QString sqlColumns();
		QString sqlBinds();

		QVariantList autoValues(PopColumn c);
		QVariantList numberValues(PopColumn c);
		QVariantList textValues(PopColumn c);

	private slots:
		void populateButton_clicked();
};

#endif
