/*
For general Sqliteman copyright and licensing information please refer
to the COPYING file provided with the program. Following this notice may exist
a copyright and/or license notice that predates the release of Sqliteman
for which a new license (GPL+exception) is in place.
*/
#include <QSqlQuery>
#include <QSqlError>
#include <QHeaderView>
#include <math.h>

#include "populatordialog.h"
#include "populatorcolumnwidget.h"


PopulatorDialog::PopulatorDialog(QWidget * parent, const QString & table, const QString & schema)
	: QDialog(parent),
	  m_schema(schema),
	  m_table(table)
{
	setupUi(this);
	columnTable->horizontalHeader()->setStretchLastSection(true);

	FieldList fields = Database::tableFields(m_table, m_schema);
	columnTable->clearContents();
	columnTable->setRowCount(fields.size());
	QRegExp sizeExp("\\(\\d+\\)");
	for(int i = 0; i < fields.size(); ++i)
	{
		Populator::PopColumn col;
		col.name = fields[i].name;
		col.type = fields[i].type;
		col.pk = fields[i].pk;
// 		col.action has to be set in PopulatorColumnWidget instance!
		if (sizeExp.indexIn(col.type) != -1)
		{
			QString s = sizeExp.capturedTexts()[0].remove("(").remove(")");
			bool ok;
			col.size = s.toInt(&ok);
			if (!ok)
				col.size = 10;
		}
		else
			col.size = 10;
		col.prefix = "";

		QTableWidgetItem * nameItem = new QTableWidgetItem(col.name);
		QTableWidgetItem * typeItem = new QTableWidgetItem(col.type);
		columnTable->setItem(i, 0, nameItem);
		columnTable->setItem(i, 1, typeItem);
		PopulatorColumnWidget *p = new PopulatorColumnWidget(col, columnTable);
		connect(p, SIGNAL(actionTypeChanged()),
				this, SLOT(checkActionTypes()));
		columnTable->setCellWidget(i, 2, p);
	}

	columnTable->resizeColumnsToContents();
	checkActionTypes();

	connect(populateButton, SIGNAL(clicked()),
			this, SLOT(populateButton_clicked()));
	connect(spinBox, SIGNAL(valueChanged(int)),
			this, SLOT(spinBox_valueChanged(int)));
}

void PopulatorDialog::spinBox_valueChanged(int)
{
	checkActionTypes();
}

void PopulatorDialog::checkActionTypes()
{
	bool enable = false;
	if (spinBox->value() != 0)
	{
		for (int i = 0; i < columnTable->rowCount(); ++i)
		{
			if (qobject_cast<PopulatorColumnWidget*>
					(columnTable->cellWidget(i, 2))->column().action != Populator::T_IGNORE)
			{
				enable = true;
				break;
			}
		}
	}
	populateButton->setEnabled(enable);
}

qlonglong PopulatorDialog::tableRowCount()
{
	QString sql("select count(1) from \"%1\".\"%2\";");
	QSqlQuery query(sql.arg(m_schema).arg(m_table),
					QSqlDatabase::database(SESSION_NAME));
	query.exec();
	if (query.lastError().isValid())
	{
		textBrowser->append(tr("Cannot get statistics for table."));
		textBrowser->append(query.lastError().text());
		return -1;
	}
	while(query.next())
		return query.value(0).toLongLong();
	return -1;
}

QString PopulatorDialog::sqlColumns()
{
	QStringList s;
	foreach (Populator::PopColumn i, m_columnList)
	{
		if (i.action != Populator::T_IGNORE)
			s.append(i.name);
	}
	return s.join("\", \"");
}

QString PopulatorDialog::sqlBinds()
{
	QStringList s;
	foreach (Populator::PopColumn i, m_columnList)
	{
		if (i.action != Populator::T_IGNORE)
			s.append(i.name);
	}
	return s.join(", :");
}

void PopulatorDialog::populateButton_clicked()
{
	qlonglong cntPre, cntPost;
	textBrowser->clear();
	m_columnList.clear();

	for (int i = 0; i < columnTable->rowCount(); ++i)
		m_columnList.append(qobject_cast<PopulatorColumnWidget*>(columnTable->cellWidget(i, 2))->column());

	QSqlQuery query(QSqlDatabase::database(SESSION_NAME));
	QString sql = "INSERT %1 INTO \"%2\".\"%3\" (\"%4\") VALUES (:%5);";
	query.prepare(sql.arg(constraintBox->isChecked() ? "OR IGNORE" : "")
			.arg(m_schema).arg(m_table)
			.arg(sqlColumns()).arg(sqlBinds()));

	cntPre = tableRowCount();

	if (!Database::execSql("BEGIN TRANSACTION;"))
	{
		textBrowser->append(tr("Begin transaction failed."));
		Database::execSql("ROLLBACK;");
		return;
	}

	foreach (Populator::PopColumn i, m_columnList)
	{
		switch (i.action)
		{
			case Populator::T_AUTO:
				query.addBindValue(autoValues(i));
				break;
			case Populator::T_NUMB:
				query.addBindValue(numberValues(i));
				break;
			case Populator::T_TEXT:
				query.addBindValue(textValues(i));
				break;
			case Populator::T_PREF:
				query.addBindValue(textPrefixedValues(i));
				break;
			case Populator::T_STAT:
				query.addBindValue(staticValues(i));
				break;
			case Populator::T_IGNORE:
				break;
		};
	}

	if (!query.execBatch())
		textBrowser->append(query.lastError().text());
	else
		textBrowser->append(tr("Data inserted."));

	if (!Database::execSql("COMMIT;"))
		textBrowser->append(tr("Transaction commit failed."));

	cntPost = tableRowCount();
	textBrowser->append(tr("It's done. Check messages above."));

	if (cntPre != -1 && cntPost != -1)
		textBrowser->append(tr("Row(s) inserted: %1").arg(cntPost-cntPre));
}

QVariantList PopulatorDialog::autoValues(Populator::PopColumn c)
{
	QString sql("select max(%1) from \"%2\".\"%3\";");
	QSqlQuery query(sql.arg(c.name).arg(m_schema).arg(m_table),
					QSqlDatabase::database(SESSION_NAME));
	query.exec();

	if (query.lastError().isValid())
	{
		textBrowser->append(tr("Cannot get MAX() for column: %1").arg(c.name));
		textBrowser->append(query.lastError().text());
		return QVariantList();
	}

	int max = 0;
	while(query.next())
		max = query.value(0).toInt();

	QVariantList ret;
	for (int i = 0; i < spinBox->value(); ++i)
		ret.append(i+max+1);

	return ret;
}

QVariantList PopulatorDialog::numberValues(Populator::PopColumn c)
{
	QVariantList ret;
	for (int i = 0; i < spinBox->value(); ++i)
		ret.append(qrand() % (int)pow(10.0, c.size));
	return ret;
}

QVariantList PopulatorDialog::textValues(Populator::PopColumn c)
{
	QVariantList ret;
	for (int i = 0; i < spinBox->value(); ++i)
	{
		QStringList l;
		for (int j = 0; j < c.size; ++j)
			l.append(QChar((qrand() % 58) + 65));
		ret.append(l.join("")
				.replace(QRegExp("(\\[|\\'|\\\\|\\]|\\^|\\_|\\`)"), " ")
				.simplified());
	}
	return ret;
}

QVariantList PopulatorDialog::textPrefixedValues(Populator::PopColumn c)
{
	QVariantList ret;
	for (int i = 0; i < spinBox->value(); ++i)
		ret.append(c.prefix + QString("%1").arg(i+1));
	return ret;
}

QVariantList PopulatorDialog::staticValues(Populator::PopColumn c)
{
	QVariantList ret;
	for (int i = 0; i < spinBox->value(); ++i)
		ret.append(c.prefix);
	return ret;
}
