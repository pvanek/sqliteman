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
#include "utils.h"


AlterTableDialog::AlterTableDialog(QWidget * parent, const QString & tableName, const QString & schema)
	: TableEditorDialog(parent),
	m_table(tableName),
	m_schema(schema),
	m_protectedRows(0),
	m_dropColumns(0)
{
	update = false;

	ui.nameEdit->setText(tableName);
// 	ui.nameEdit->setDisabled(true);
	ui.databaseCombo->addItem(schema);
	ui.databaseCombo->setDisabled(true);
	ui.tabWidget->removeTab(1);
	ui.adviceLabel->hide();
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
// 		typeItem->setFlags(Qt::ItemIsSelectable); // TODO: change afinity in ALTER TABLE too!
		QTableWidgetItem * defItem = new QTableWidgetItem(fields[i].defval);
		QTableWidgetItem * ixItem = new QTableWidgetItem();

		QCheckBox * dropItem = new QCheckBox(this);
		connect(dropItem, SIGNAL(stateChanged(int)),
				this, SLOT(dropItem_stateChanged(int)));

		ixItem->setFlags(Qt::ItemIsSelectable);
		if (m_columnIndexMap.contains(fields[i].name))
		{
			ixItem->setIcon(Utils::getIcon("index.png"));
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
// 	ui.columnTable->resizeColumnsToContents();
	checkChanges();
}

void AlterTableDialog::dropItem_stateChanged(int state)
{
	state == Qt::Checked ? ++m_dropColumns : --m_dropColumns;
	checkChanges();
}

bool AlterTableDialog::execSql(const QString & statement, const QString & message, const QString & tmpName)
{
	QSqlQuery query(statement, QSqlDatabase::database(SESSION_NAME));
	if(query.lastError().isValid())
	{
		ui.resultEdit->append(QString("%1 (%2) %3:").arg(message)
													.arg(tr("failed"))
													.arg(query.lastError().text()));
		ui.resultEdit->append(statement);
		if (!tmpName.isNull())
			ui.resultEdit->append(tr("Old table is stored as %1").arg(tmpName));
		return false;
	}
	ui.resultEdit->append(message);
	return true;
}

QStringList AlterTableDialog::originalSource()
{
	QString ixsql("select sql from \"%1\".sqlite_master where type in ('index', 'trigger') and tbl_name = '%2';");
	QSqlQuery query(ixsql.arg(m_schema).arg(m_table), QSqlDatabase::database(SESSION_NAME));
	QStringList ret;

	if (query.lastError().isValid())
	{
		ui.resultEdit->append(tr("Cannot get index list. %1").arg(query.lastError().text()));
		return QStringList();
	}
	while(query.next())
		ret.append(query.value(0).toString());
	return ret;
}

bool AlterTableDialog::renameTable()
{
	QString newTableName(ui.nameEdit->text().trimmed());
	if (m_table == newTableName)
		return true;
	
	QString sql = QString("ALTER TABLE \"%1\".\"%2\" RENAME TO \"%3\";")
			.arg(m_schema)
			.arg(m_table)
			.arg(newTableName);
	if (execSql(sql, tr("Renaming the table \"%1\" to \"%2\".").arg(m_table).arg(newTableName)))
	{
		m_table = newTableName;
		return true;
	}
	return false;
}

void AlterTableDialog::createButton_clicked()
{
	ui.resultEdit->clear();
	// rename table if it's required
	if (!renameTable())
		return;

	// drop columns first
// 	if (m_dropColumns > 0)
	{
		QStringList existingObjects = Database::getObjects().keys();
		// indexes and triggers on the original table
		QStringList originalSrc = originalSource();

		// generate unique temporary tablename
		QString tmpName("_alter%1_" + m_table);
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

		if (!execSql(QString("ALTER TABLE \"%1\".\"%2\" RENAME TO \"%3\";")
							.arg(m_schema).arg(m_table).arg(tmpName),
					 tr("Rename original table to %1").arg(tmpName)))
		{
			return;
		}

		QString sql = QString("CREATE TABLE %1 (\n").arg(m_table);
		QStringList tmpInsertColumns;
		foreach (DatabaseTableField f, newColumns)
		{
			sql += getColumnClause(f);
			tmpInsertColumns.append(f.name);
		}
		sql = sql.remove(sql.size() - 2, 2); // cut the extra ", "
		sql += "\n);\n";

		if (!execSql(sql, tr("Creating new table: %1").arg(m_table)))
			return;

		update = true;

		// insert old data
		if (!execSql("BEGIN TRANSACTION;", tr("Begin Transaction"), tmpName))
		{
			Database::dropTable(tmpName, "main");
			return;
		}
		QString insSql(QString("INSERT INTO \"%1\".\"%2\" (\"%3\") SELECT \"%4\" FROM \"%5\";")
								.arg(m_schema).arg(m_table)
								.arg(tmpInsertColumns.join("\",\""))
								.arg(tmpInsertColumns.join("\",\""))
								.arg(tmpName));
		if (!execSql(insSql, tr("Data Transfer"), tmpName))
			return;
		if (!execSql("COMMIT;", tr("Transaction Commit"), tmpName))
			return;

		// drop old table
		if (!execSql(QString("DROP TABLE \"%1\";").arg(tmpName),
			 tr("Dropping original table %1").arg(tmpName),
				tmpName))
			return;

		// restoring original indexes
		foreach (QString restoreSql, originalSrc)
			execSql(restoreSql, tr("Recreating original index/trigger"));
	}

	// handle add columns
	if (addColumns())
		update = true;

	if (update)
	{
		resetStructure();
		ui.resultEdit->append(tr("Alter Table Done"));
	}
}

bool AlterTableDialog::addColumns()
{
	// handle new columns
	DatabaseTableField f;
	QString sql("ALTER TABLE \"%1\".\"%2\" ADD COLUMN \"%3\" %4 %5 %6;");
	QString nn;
	QString def;
	QString fullSql;

	// only if it's required to do
	if (m_protectedRows == ui.columnTable->rowCount())
		return true;

	for(int i = m_protectedRows; i < ui.columnTable->rowCount(); i++)
	{
		f = getColumn(i);
		if (f.cid == -1)
			continue;

		nn = f.notnull ? " NOT NULL" : "";
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
										.arg(query.lastError().text())
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
// 	ui.createButton->setEnabled(m_dropColumns > 0 || m_protectedRows < ui.columnTable->rowCount());
	ui.createButton->setEnabled(true);
}
