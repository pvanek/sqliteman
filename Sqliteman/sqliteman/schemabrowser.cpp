/*
For general Sqliteman copyright and licensing information please refer
to the COPYING file provided with the program. Following this notice may exist
a copyright and/or license notice that predates the release of Sqliteman
for which a new license (GPL+exception) is in place.
*/
#include <QInputDialog>

#include "schemabrowser.h"
#include "database.h"

SchemaBrowser::SchemaBrowser(QWidget * parent, Qt::WindowFlags f)
	: QWidget(parent, f)
{
	setupUi(this);

// 	connect(pragmaTable, SIGNAL(currentCellChanged(int, int, int, int)),
// 			this, SLOT(pragmaTable_currentCellChanged(int, int, int, int)));
	connect(setPragmaButton, SIGNAL(clicked()), this, SLOT(setPragmaButton_clicked()));
}

void SchemaBrowser::buildPragmasTree()
{
	disconnect(pragmaTable, SIGNAL(currentCellChanged(int, int, int, int)),
			   this, SLOT(pragmaTable_currentCellChanged(int, int, int, int)));
	pragmaTable->clearContents();
	pragmaTable->setRowCount(0);

	addPragma("auto_vacuum");
	addPragma("cache_size");
	addPragma("case_sensitive_like");
	addPragma("count_changes");
	addPragma("default_cache_size");
	addPragma("default_synchronous");
	addPragma("empty_result_callbacks");
	addPragma("encoding");
	addPragma("full_column_names");
	addPragma("fullfsync");
	addPragma("legacy_file_format");
	addPragma("locking_mode");
	addPragma("page_size");
	addPragma("max_page_count");
	addPragma("read_uncommitted");
	addPragma("short_column_names");
	addPragma("synchronous");
	addPragma("temp_store");
	addPragma("temp_store_directory");

	pragmaTable_currentCellChanged(0, 0, 0, 0);
	connect(pragmaTable, SIGNAL(currentCellChanged(int, int, int, int)),
		    this, SLOT(pragmaTable_currentCellChanged(int, int, int, int)));
}

void SchemaBrowser::addPragma(const QString & name)
{
	int row = pragmaTable->rowCount();
	pragmaTable->setRowCount(row + 1);
	pragmaTable->setItem(row, 0, new QTableWidgetItem(name));
	pragmaTable->setItem(row, 1, new QTableWidgetItem(Database::pragma(name)));
}

void SchemaBrowser::pragmaTable_currentCellChanged(int currentRow, int /*currentColumn*/, int /*previousRow*/, int /*previousColumn*/)
{
	pragmaName->setText(pragmaTable->item(currentRow, 0)->text());
	pragmaValue->setText(pragmaTable->item(currentRow, 1)->text());
}

void SchemaBrowser::setPragmaButton_clicked()
{
	int row = pragmaTable->currentRow();
	QTableWidgetItem * item = pragmaTable->item(row, 0);
	QString text(item->text().toLower());
	QString value(pragmaTable->item(row, 1)->text());
	// TODO: create a better way to get new value.
	// TODO: Check sane values etc. It will need a some description of pragmas (XML?... now I wisth it could be written in python dicts...
	bool ok;
	QString newValue = QInputDialog::getText(this, "Set Pragma", text, QLineEdit::Normal, value, &ok);
	if (ok && !text.isEmpty())
	{
		Database::execSql(QString("PRAGMA main.%1 = %2;").arg(text).arg(newValue));
		buildPragmasTree();
	}
}
