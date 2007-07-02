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

#include <QRadioButton>
#include <QPushButton>
#include <QGroupBox>
#include <QCheckBox>
#include <QLabel>

#include <QGridLayout>
#include <QHBoxLayout>
#include <QVBoxLayout>

#include "queryeditordialog.h"


TermEditor::TermEditor(const FieldList & fieldList): QWidget(0)
{
	fields = new QComboBox();
	for(int i = 0; i < fieldList.size(); i++)
		fields->addItem(fieldList[i].name);

	relations = new QComboBox();
	relations->addItems(QStringList() << tr("Contains") << tr("Doesn't contain") << tr("Equals") << tr("Not equals")
					<< tr("Bigger than") << tr("Smaller than"));

	value = new QLineEdit();

	QHBoxLayout * layout = new QHBoxLayout();
	layout->addWidget(fields);
	layout->addWidget(relations);
	layout->addWidget(value);

	setLayout(layout);
}


/*!
 * @brief Creates the query editor.
 * 
 * @param parent The parent widget for the dialog.
 */
QueryEditorDialog::QueryEditorDialog(Mode mode, QWidget * parent): QDialog(parent)
{
	curMode = mode;
	m_schema = "main"; // FIXME: real schema
	
	initUI();
	
	QStringList tables = Database::getObjects("table").keys();
	tableList->addItems(tables);
	
	// If a database has at least one table. auto select it
	if(tables.size() > 0)
		tableSelected(tables[0]); 
	
	if(mode == CreateView)
		setWindowTitle(tr("Create View"));
	else
		setWindowTitle(tr("Build Query"));
}

QueryEditorDialog::~QueryEditorDialog()
{
}

/*!
 * @brief generates a valid SQL statement using the values in the dialog
 */
QString QueryEditorDialog::statement()
{
	QString logicWord;
	QString sql = "SELECT ";
	
	// Add checked fields list
	for(int i = 0; i < checkLayout->count(); i++)
	{
		QWidget * widget = checkLayout->itemAt(i)->widget();
		
		if(!widget)
			continue;
		
		QCheckBox * check = qobject_cast<QCheckBox *>(widget);
		if(check)
			if(check->checkState() == Qt::Checked)
				sql += (check->text() + ", ");
	}
	sql = sql.remove(sql.size() - 2, 2); 	// cut the extra ", "
	
	// Add table name
	sql += (" FROM '" + tableList->currentText() + "'");
	
	// Optionaly add terms
	if(termsLayout->count() > 0)
	{
		// But first determine what is the chosen logic word (And/Or)
		(andButton->isChecked()) ? logicWord = "AND" : logicWord = "OR";
		
		sql += " WHERE ";
		
		for(int i = 0; i < termsLayout->count(); i++)
		{
			QWidget * widget = termsLayout->itemAt(i)->widget();
		
			if(!widget)
				continue;
		
			TermEditor * term = qobject_cast<TermEditor *>(widget);
			if(term)
			{
				sql += term->selectedField();
				
				switch(term->selectedRelation())
				{
					case 0:		// Contains
						sql += (" LIKE '%" + term->selectedValue() + "%'");
						break;
						
					case 1: 	// Doesn't contain
						sql += (" NOT LIKE '%" + term->selectedValue() + "%'");
						break;
							
					case 2:		// Equals
						sql += (" = '" + term->selectedValue() + "'");
						break;
						
					case 3:		// Not equals
						sql += (" <> '" + term->selectedValue() + "'");
						break;
						
					case 4:		// Bigger than
						sql += (" > '" + term->selectedValue() + "'");	
						break;
						
					case 5:		// Smaller than
						sql += (" < '" + term->selectedValue() + "'");	
						break;
				}
			}
			sql += (" " + logicWord + " ");
		}
		sql = sql.remove(sql.size() - (logicWord.size() + 2), logicWord.size() + 2); // cut the extra " AND " or " OR "
	}
	sql += ";";
	
	return sql;
}

QString QueryEditorDialog::viewName()
{
	return viewNameEdit->text();
}

void QueryEditorDialog::initUI()
{
	QPushButton * okButton = new QPushButton(tr("OK"));
	QPushButton * cancelButton = new QPushButton(tr("Cancel"));

	connect(okButton, SIGNAL(clicked()), this, SLOT(accept()));
	connect(cancelButton, SIGNAL(clicked()), this, SLOT(reject()));
	
	QLabel * viewLabel = new QLabel(tr("View name: "));
	viewNameEdit = new QLineEdit();
	
	QLabel * tableLabel = new QLabel(tr("Table to Query: "));
	tableList = new QComboBox();
	
	connect(tableList, SIGNAL(activated(const QString &)), this, SLOT(tableSelected(const QString &)));
	
	andButton = new QRadioButton(tr("Match all of the following terms"));
	orButton = new QRadioButton(tr("Match any of the following terms"));
	
	andButton->setChecked(true);
	
	QPushButton * moreButton = new QPushButton(tr("More"));
	lessButton = new QPushButton(tr("Less"));
	
	lessButton->setEnabled(false);
	
	connect(moreButton, SIGNAL(clicked()), this, SLOT(moreTerms()));
	connect(lessButton, SIGNAL(clicked()), this, SLOT(lessTerms()));
	
	//
	// Layout
	//
	checkLayout = new QGridLayout();
	termsLayout = new QVBoxLayout();
	
	QGridLayout * topLayout = new QGridLayout();
	
	if(curMode == CreateView)
	{
		topLayout->addWidget(viewLabel, 0, 0);
		topLayout->addWidget(viewNameEdit, 0, 1);
	}
	topLayout->addWidget(tableLabel, 1, 0);
	topLayout->addWidget(tableList, 1, 1);
	
	QGroupBox * fieldsBox = new QGroupBox(tr("Fields"));
	fieldsBox->setLayout(checkLayout);
	
	QHBoxLayout * andOrLayout = new QHBoxLayout();
	andOrLayout->addWidget(andButton);
	andOrLayout->addWidget(orButton);
	
	QHBoxLayout * termButtonsLayout = new QHBoxLayout();
	termButtonsLayout->addStretch(1);
	termButtonsLayout->addWidget(moreButton);
	termButtonsLayout->addWidget(lessButton);
	
	QVBoxLayout * outerTermsLayout = new QVBoxLayout();
	outerTermsLayout->addLayout(andOrLayout);
	outerTermsLayout->addLayout(termsLayout, 1);
	outerTermsLayout->addLayout(termButtonsLayout);
	
	QGroupBox * termsBox = new QGroupBox(tr("Terms"));
	termsBox->setLayout(outerTermsLayout);
	
	QHBoxLayout * buttonLayout = new QHBoxLayout();
	buttonLayout->addStretch(1);
	buttonLayout->addWidget(okButton);
	buttonLayout->addWidget(cancelButton);

	QVBoxLayout * mainLayout = new QVBoxLayout();
	mainLayout->addLayout(topLayout);
	mainLayout->addWidget(fieldsBox);
	mainLayout->addWidget(termsBox);
	mainLayout->addStretch(1);
	mainLayout->addLayout(buttonLayout);
	
	setLayout(mainLayout);
}

void QueryEditorDialog::tableSelected(const QString & table)
{
	FieldList fields = Database::tableFields(table, m_schema);
	
	curTable = table;
	
	// Clear checkLayout
	QLayoutItem * child;
	while((child = checkLayout->takeAt(0)) != 0) 
	{
		delete child->widget();
		delete child;
	}
	repaint(0, 0, height(), width());
	
	int row = 0;
	for(int i = 0; i < fields.size(); i++)
	{
		DatabaseTableField field = fields[i];
		QCheckBox * check = new QCheckBox(field.name);
		
		check->setCheckState(Qt::Checked);
		checkLayout->addWidget(check, row, i % 2);
		
		if((i + 1) % 2 == 0)
			row++;
	}
}

void QueryEditorDialog::moreTerms()
{
	TermEditor * term = new TermEditor(Database::tableFields(curTable, m_schema));
	
	termsLayout->addWidget(term);
	lessButton->setEnabled(true);
}

void QueryEditorDialog::lessTerms()
{
	QLayoutItem * child = termsLayout->takeAt(termsLayout->count() - 1);
	if(child)
	{
		delete child->widget();
		delete child;
	}
	
	if(termsLayout->count() == 0)
		lessButton->setEnabled(false);
}
