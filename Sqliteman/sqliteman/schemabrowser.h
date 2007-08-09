/*
For general Sqliteman copyright and licensing information please refer
to the COPYING file provided with the program. Following this notice may exist
a copyright and/or license notice that predates the release of Sqliteman
for which a new license (GPL+exception) is in place.
*/

#ifndef SCHEMABROWSER_H
#define SCHEMABROWSER_H

#include "ui_schemabrowser.h"


/*! \brief A "toolbox" widget containing DB objects and more useful info.
It contains a DB object tree and PRAGMAs list now.
\author Petr Vanek <petr@scribus.info>
*/
class SchemaBrowser : public QWidget, public Ui::SchemaBrowser
{
	Q_OBJECT

	public:
		SchemaBrowser(QWidget * parent = 0, Qt::WindowFlags f = 0);

		//! \brief Clear and reset the pragma values.
		void buildPragmasTree();

	private:
		/*! \brief Add a pragma into the list (QTableWidget).
		Query the DB for its value and store it in the widget.
		\param name name of the pragma (PRAGMA name;)
		*/
		void addPragma(const QString & name);

	private slots:
		//! \brief Show currently selected pragma in the detail widget.
		void pragmaTable_currentCellChanged(int currentRow, int currentColumn, int previousRow, int previousColumn);
		//! \brief Set new value for the chosen pragma.
		void setPragmaButton_clicked();
};

#endif
