/*
For general Sqliteman copyright and licensing information please refer
to the COPYING file provided with the program. Following this notice may exist
a copyright and/or license notice that predates the release of Sqliteman
for which a new license (GPL+exception) is in place.
*/
#include <QSqlQuery>
#include <QSqlError>
#include "populatordialog.h"

#include <QtDebug>

#define T_AUTO 0
#define T_NUMB 1
#define T_TEXT 2


PopulatorDialog::PopulatorDialog(QWidget * parent, const QString & table, const QString & schema)
	: QDialog(parent),
	  m_schema(schema),
	  m_table(table)
{
	setupUi(this);

	actionMap[T_AUTO] = tr("Autoincrement");
	actionMap[T_NUMB] = tr("Random Number");
	actionMap[T_TEXT] = tr("Random Text");

	FieldList fields = Database::tableFields(m_table, m_schema);
	columnTable->clearContents();
	columnTable->setRowCount(fields.size());
	for(int i = 0; i < fields.size(); ++i)
	{
		PopColumn col;
		col.name = fields[i].name;
		col.type = fields[i].type;
		col.pk = fields[i].pk;
		col.action = defaultSuggestion(fields[i]);
		col.autoCounter = -1;
		col.size = 0;
		columnList.append(col);

		QTableWidgetItem * nameItem = new QTableWidgetItem(fields[i].name);
		QTableWidgetItem * typeItem = new QTableWidgetItem(fields[i].type);
		QTableWidgetItem * suggestedItem = new QTableWidgetItem(actionMap[col.action]);
		columnTable->setItem(i, 0, nameItem);
		columnTable->setItem(i, 1, typeItem);
		columnTable->setItem(i, 2, suggestedItem);
	}

	columnTable->resizeColumnsToContents();
	connect(populateButton, SIGNAL(clicked()), this, SLOT(populateButton_clicked()));
}

int PopulatorDialog::defaultSuggestion(const DatabaseTableField & column)
{
	QString t(column.type);
	t = t.remove(QRegExp("\\(\\d+\\)")).toUpper().simplified();

	if (column.pk)
		return T_AUTO;

	if (t == "INTEGER" || t == "NUMBER")
		return T_NUMB;
	else
		return T_TEXT;
}

QString PopulatorDialog::sqlColumns()
{
	QStringList s;
	foreach (PopColumn i, columnList)
		s.append(i.name);
	return s.join("\", \"");
}

QString PopulatorDialog::sqlBinds()
{
	QStringList s;
	foreach (PopColumn i, columnList)
		s.append(i.name);
	return s.join(", :");
}

void PopulatorDialog::populateButton_clicked()
{
	textBrowser->clear();

	if (spinBox->value() == 0)
	{
		textBrowser->append(tr("Specify count of the rows to insert."));
		return;
	}

	QSqlQuery query(QSqlDatabase::database(SESSION_NAME));
	QString sql = "INSERT INTO \"%1\".\"%2\" (\"%3\") VALUES (:%4);";
	query.prepare(sql.arg(m_schema).arg(m_table).arg(sqlColumns()).arg(sqlBinds()));
	qDebug() << query.lastQuery();

	
	if (!Database::execSql("BEGIN TRANSACTION;"))
	{
		textBrowser->append(tr("Begin transaction failed."));
		return;
	}

	foreach (PopColumn i, columnList)
	{
		switch (i.action)
		{
			case T_AUTO:
				query.addBindValue(autoValues(i));
				break;
			case T_NUMB:
				query.addBindValue(numberValues(i));
				break;
			case T_TEXT:
				query.addBindValue(textValues(i));
				break;
		};
	}

	if (!query.execBatch())
	{
		textBrowser->append(query.lastError().databaseText());
	}
	textBrowser->append(tr("Affected rows: %1").arg(query.numRowsAffected()));

	if (!Database::execSql("COMMIT;"))
	{
		textBrowser->append(tr("Transaction commit failed."));
		return;
	}
	accept();
}

QVariantList PopulatorDialog::autoValues(PopColumn c)
{
	QString sql("select max(%1) from \"%2\".\"%3\";");
	QSqlQuery query(sql.arg(c.name).arg(m_schema).arg(m_table),
					QSqlDatabase::database(SESSION_NAME));
	query.exec();

	if (query.lastError().isValid())
	{
		textBrowser->append(tr("Cannot get MAX() for column: %1").arg(c.name));
		textBrowser->append(query.lastError().databaseText());
		return QVariantList();
	}

	int max = 0;
	while(query.next())
		max = query.value(0).toInt();

	QVariantList ret;
	for (int i = 0; i < spinBox->value(); ++i)
		ret.append(i+max+1);
	qDebug() << "autoValues: " << ret;
	return ret;
}

QVariantList PopulatorDialog::numberValues(PopColumn c)
{
	QVariantList ret;
	for (int i = 0; i < spinBox->value(); ++i)
		ret.append(qrand());
	qDebug() << "numberValues: " << ret;
	return ret;
}

QVariantList PopulatorDialog::textValues(PopColumn c)
{
	QVariantList ret;
	for (int i = 0; i < spinBox->value(); ++i)
		ret.append("TODO");
	qDebug() << "textValues: " << ret;
	return ret;
}
