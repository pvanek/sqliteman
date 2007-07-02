/*
 * This file is part of LiteMan.
 *
 * Copyright 2006 Igor Khanin
 *
 * LiteMan is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * LiteMan is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with LiteMan; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */

#ifndef QUERYEDITORDIALOG_H
#define QUERYEDITORDIALOG_H

#include <QDialog>
#include <QComboBox>
#include <QLineEdit>

#include "database.h"

class QGridLayout;
class QVBoxLayout;
class QRadioButton;
class QPushButton;


/*! 
 * @brief A hidden helper widget for editing a query term
 */
class TermEditor : public QWidget
{
	Q_OBJECT

	public:
		TermEditor(const FieldList & fieldList);
		
		QString selectedField() { return fields->currentText(); };
		int selectedRelation() { return relations->currentIndex(); };
		QString selectedValue() { return value->text(); };
		
	private:
		QComboBox * fields;
		QComboBox * relations;
		QLineEdit * value;
};

/*!
 * @brief A dialog for creating and editing queries
 * \author Igor Khanin
 */
class QueryEditorDialog : public QDialog
{
		Q_OBJECT
	public:
		typedef enum
		{
			BuildQuery,
			CreateView
		}
		Mode;	
				
	public:
		QueryEditorDialog(Mode mode, QWidget * parent = 0);
		~QueryEditorDialog();
		
		QString statement();
		QString viewName();
		
	private:
		void initUI();
		QString m_schema;
		
	private slots:
		void tableSelected(const QString & table);
		void moreTerms();
		void lessTerms();
		
	private:
		Mode curMode;
		QString curTable;
		
		QPushButton * lessButton;
		QRadioButton * andButton;
		QRadioButton * orButton;
		
		QComboBox * tableList;
		QLineEdit * viewNameEdit;
		QGridLayout * checkLayout;
		QVBoxLayout * termsLayout;
};

#endif //QUERYEDITORDIALOG_H
