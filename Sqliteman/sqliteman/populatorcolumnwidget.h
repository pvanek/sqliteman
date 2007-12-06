/*
For general Sqliteman copyright and licensing information please refer
to the COPYING file provided with the program. Following this notice may exist
a copyright and/or license notice that predates the release of Sqliteman
for which a new license (GPL+exception) is in place.
*/
#ifndef POPULATORCOLUMNWIDGET_H
#define POPULATORCOLUMNWIDGET_H

#include <QWidget>

#include "ui_populatorcolumnwidget.h"

#include "populatorstructs.h"


/*! \brief Populator configurator for one column.
It's a GUI for column related configuration. No calculation
are handled here -- only Populator::PopColumn settings.

\author Petr Vanek <petr@scribus.ifno>
*/
class PopulatorColumnWidget :
		public QWidget,
		public Ui::PopulatorColumnWidget
{
	Q_OBJECT

	public:
		PopulatorColumnWidget(Populator::PopColumn column,
							  QWidget * parent = 0);

		Populator::PopColumn column() { return m_column; };

	signals:
		/*! Emitted in every actionCombo_currentIndexChanged()
		to allow populator to run in the main dialog. */
		void actionTypeChanged();

	private:
		Populator::PopColumn m_column;
		//! Guess what it can insert as value by column datatype
		int defaultSuggestion();

	private slots:
		void actionCombo_currentIndexChanged(int);
		void specEdit_textChanged(const QString &);
};


#endif
