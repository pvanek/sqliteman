/*
For general Sqliteman copyright and licensing information please refer
to the COPYING file provided with the program. Following this notice may exist
a copyright and/or license notice that predates the release of Sqliteman
for which a new license (GPL+exception) is in place.
*/

#ifndef SQLITEMVIEW_H
#define SQLITEMVIEW_H

#include "ui_sqlitemview.h"

class QDataWidgetMapper;
class QAbstractItemModel;
class QLineEdit;


/*! \brief Display one record in one form view.
It provides set of generic line edits for current model index.
User can change model indexes by GUI buttons.
\author Petr Vanek <petr@scribus.info>
*/
class SqlItemView : public QWidget, public Ui::SqlItemView
{
	Q_OBJECT

	public:
		SqlItemView(QWidget * parent = 0);

		/*! \brief Set the model and connect the GUI widgets to model's items.
		All previously generated GUI widgets are deleted and recreated
		again depending on the new model QSqlRecord structure. */
		void setModel(QAbstractItemModel * model);
		QAbstractItemModel * model();
		
		void setCurrentIndex(int);
		int currentIndex();

	private:
		/*! \brief A "shadow" widget for new layout recreated every time is the model set.
		This widget is owned by scroll area. It's deleted and re-created on every setModel()
		call. */
		QWidget * layoutWidget;
		//! \brief Mapping tool for model-generic widgets relations. See Qt4 docs.
		QDataWidgetMapper *m_mapper;

	private slots:
		//! \brief Set the navigation buttons state and "X of Y" label.
		void updateButtons(int row);
};

#endif
