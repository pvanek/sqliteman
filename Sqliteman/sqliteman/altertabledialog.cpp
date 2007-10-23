/*
For general Sqliteman copyright and licensing information please refer
to the COPYING file provided with the program. Following this notice may exist
a copyright and/or license notice that predates the release of Sqliteman
for which a new license (GPL+exception) is in place.
*/

#include <QCheckBox>
#include <QSqlQuery>
#include <QSqlError>

#include "altertabledialog.h"

#include <QtDebug>
AlterTableDialog::AlterTableDialog(QWidget * parent, const QString & tableName, const QString & schema)
	: TableEditorDialog(parent),
	m_table(tableName),
	m_schema(schema),
	m_protectedRows(0),
	m_dropColumns(0)
{
	update = false;

	ui.nameEdit->setText(tableName);
	ui.nameEdit->setDisabled(true);
	ui.databaseCombo->addItem(schema);
	ui.databaseCombo->setDisabled(true);
	ui.tabWidget->removeTab(1);
	ui.createButton->setText(tr("Alte&r"));
	ui.removeButton->setEnabled(false);
	setWindowTitle(tr("Alter Table"));

	ui.columnTable->insertColumn(4); // show if it's indexed
	QTableWidgetItem * captIx = new QTableWidgetItem(tr("Indexed"));
	ui.columnTable->setHorizontalHeaderItem(4, captIx);
	ui.columnTable->insertColumn(5); // drop protected columns
	QTableWidgetItem * captDrop = new QTableWidgetItem(tr("Drop"));
	ui.columnTable->setHorizontalHeaderItem(5, captDrop);

	connect(ui.columnTable, SIGNAL(cellClicked(int, int)), this, SLOT(cellClicked(int,int)));

	resetStructure();
}

void AlterTableDialog::resetStructure()
{
	// obtain all indexed colums for DROP COLUMN checks
	foreach(QString index, Database::getObjects("index", m_schema).values(m_table))
	{
		foreach(QString indexColumn, Database::indexFields(index, m_schema))
			m_columnIndexMap[indexColumn].append(index);
	}

	// Initialize fields
	FieldList fields = Database::tableFields(m_table, m_schema);
	ui.columnTable->clearContents();
	ui.columnTable->setRowCount(fields.size());
	for(int i = 0; i < fields.size(); i++)
	{
		QTableWidgetItem * nameItem = new QTableWidgetItem(fields[i].name);
		QTableWidgetItem * typeItem = new QTableWidgetItem(fields[i].type);
		QTableWidgetItem * defItem = new QTableWidgetItem(fields[i].defval);
		QTableWidgetItem * ixItem = new QTableWidgetItem();

		QCheckBox * dropItem = new QCheckBox(this);
		connect(dropItem, SIGNAL(stateChanged(int)),
				this, SLOT(dropItem_stateChanged(int)));

		ixItem->setFlags(Qt::ItemIsSelectable);
		if (m_columnIndexMap.contains(fields[i].name))
		{
			ixItem->setIcon(QIcon(QPixmap(QString(ICON_DIR) + "/index.png")));
			ixItem->setText(tr("Yes"));
		}
		else
			ixItem->setText(tr("No"));

		ui.columnTable->setItem(i, 0, nameItem);
		ui.columnTable->setItem(i, 1, typeItem);
		QCheckBox *nn = new QCheckBox(this);
		nn->setCheckState(fields[i].notnull ? Qt::Checked : Qt::Unchecked);
		ui.columnTable->setCellWidget(i, 2, nn);
		ui.columnTable->setItem(i, 3, defItem);
		ui.columnTable->setItem(i, 4, ixItem);
		ui.columnTable->setCellWidget(i, 5, dropItem);
	}

	m_protectedRows = ui.columnTable->rowCount();
	m_dropColumns = 0;
	ui.columnTable->resizeColumnsToContents();
	checkChanges();
}

void AlterTableDialog::dropItem_stateChanged(int state)
{
	state == Qt::Checked ? ++m_dropColumns : --m_dropColumns;
	checkChanges();
}

void AlterTableDialog::createButton_clicked()
{
	// drop columns first
	if (m_dropColumns > 0)
	{
		QStringList existingObjects = Database::getObjects().keys();

		// generate unique temporary tablename
		QString tmpName("_sqliteman_alter_%1");
		int tmpCount = 0;
		while (existingObjects.contains(tmpName.arg(tmpCount), Qt::CaseInsensitive))
			++tmpCount;
		tmpName = tmpName.arg(tmpCount);

		// create temporary table without selected columns
		FieldList newColumns;
		for(int i = 0; i < m_protectedRows; ++i)
		{
			if (!qobject_cast<QCheckBox*>(ui.columnTable->cellWidget(i, 5))->isChecked())
				newColumns.append(getColumn(i));
		}
		QString sql = QString("CREATE TABLE %1 (\n").arg(tmpName);
		QStringList tmpInsertColumns;
		foreach (DatabaseTableField f, newColumns)
		{
			sql += getColumnClause(f);
			tmpInsertColumns.append(f.name);
		}
		sql = sql.remove(sql.size() - 2, 2); 	// cut the extra ", "
		sql += "\n);\n";
		qDebug() << sql;

		QSqlQuery query(sql, QSqlDatabase::database(SESSION_NAME));
		if(query.lastError().isValid())
		{
			ui.resultEdit->setText(tr("Error while creating temporary table: %1.\n\n%2").arg(query.lastError().databaseText()).arg(sql));
			return;
		}
		update = true;
		ui.resultEdit->setText(tr("Temporary table created successfully"));

		// insert old data
		query.exec("BEGIN TRANSACTION;");
		if(query.lastError().isValid())
		{
			ui.resultEdit->append(tr("Error while data transfer. %1").arg(query.lastError().databaseText()));
			return;
		}
		ui.resultEdit->append(tr("Beging Transaction..."));

		QString insSql(QString("INSERT INTO %1 (%2) SELECT %3 FROM \"%4\".\"%5\";")
								.arg(tmpName).arg(tmpInsertColumns.join(","))
								.arg(tmpInsertColumns.join(","))
								.arg(m_schema).arg(m_table));
		query.exec(insSql);
		if(query.lastError().isValid())
		{
			ui.resultEdit->append(tr("Error while data transfer. %1\n%2").arg(query.lastError().databaseText()).arg(insSql));
			Database::dropTable(tmpName, "main");
			return;
		}
		ui.resultEdit->append(tr("Data Transfered..."));
		query.exec("COMMIT;");
		if(query.lastError().isValid())
		{
			ui.resultEdit->append(tr("Error while data transfer. %1").arg(query.lastError().databaseText()));
			Database::dropTable(tmpName, "main");
			return;
		}
		ui.resultEdit->append(tr("Commited..."));
		query.exec(QString("DROP TABLE \"%1\".\"%2\";").arg(m_schema).arg(m_table));
		if(query.lastError().isValid())
		{
			ui.resultEdit->append(tr("Error while data transfer. %1").arg(query.lastError().databaseText()));
			Database::dropTable(tmpName, "main");
			return;
		}
		ui.resultEdit->append(tr("Original table dropped..."));

		query.exec(QString("ALTER TABLE \"%1\" RENAME TO \"%2\";").arg(tmpName).arg(m_table));
		if(query.lastError().isValid())
		{
			ui.resultEdit->append(tr("Error while data transfer. %1").arg(query.lastError().databaseText()));
			Database::dropTable(tmpName, "main");
			return;
		}
		ui.resultEdit->append(tr("Temporary table renamed..."));
	}

	// handle add columns
	if (alterTable())
		update = true;

	if (update)
		resetStructure();
}

bool AlterTableDialog::alterTable()
{
	// handle new columns
	DatabaseTableField f;
	QString sql("ALTER TABLE \"%1\".\"%2\" ADD COLUMN \"%3\" %4 %5 %6;");
	QString nn;
	QString def;
	QString fullSql;

	for(int i = m_protectedRows; i < ui.columnTable->rowCount(); i++)
	{
		f = getColumn(i);
		if (f.cid == -1)
			continue;

		nn = f.notnull ? " NOT NULL" : "";
// 		def = f.defval.length() > 0 ? QString(" DEFAULT (%1)").arg(f.defval) : "";
		def = getDefaultClause(f.defval);

		fullSql = sql.arg(ui.databaseCombo->currentText())
					.arg(m_table)
					.arg(f.name)
					.arg(f.type)
					.arg(nn)
					.arg(def);

		QSqlQuery query(fullSql, QSqlDatabase::database(SESSION_NAME));
		if(query.lastError().isValid())
		{
			ui.resultEdit->setText(tr("Error while altering table %1: %2.\n%3")
										.arg(m_table)
										.arg(query.lastError().databaseText())
										.arg(fullSql));
			return false;
		}
	}
	ui.resultEdit->append(tr("Columns added successfully"));
	return true;
}

void AlterTableDialog::addField()
{
	TableEditorDialog::addField();
	checkChanges();
}

void AlterTableDialog::removeField()
{
	if (ui.columnTable->currentRow() < m_protectedRows)
		return;
	TableEditorDialog::removeField();
	checkChanges();
}

void AlterTableDialog::fieldSelected()
{
	if (ui.columnTable->currentRow() < m_protectedRows)
	{
		ui.removeButton->setEnabled(false);
		return;
	}
	TableEditorDialog::fieldSelected();
}

void AlterTableDialog::cellClicked(int row, int)
{
	if (row < m_protectedRows)
	{
		ui.removeButton->setEnabled(false);
		return;
	}
	TableEditorDialog::fieldSelected();
}

void AlterTableDialog::checkChanges()
{
	ui.createButton->setEnabled(m_dropColumns > 0 || m_protectedRows < ui.columnTable->rowCount());
}
