/*
For general Sqliteman copyright and licensing information please refer
to the COPYING file provided with the program. Following this notice may exist
a copyright and/or license notice that predates the release of Sqliteman
for which a new license (GPL+exception) is in place.
*/
#include <QFileDialog>
#include <QMessageBox>

#if QT_VERSION >= 0x040300
#include <QXmlStreamReader>
#else
#warning "QXmlStreamReader is disabled. Qt 4.3.x required."
#endif

#include <QSqlQuery>
#include <QSqlError>
#include <QtDebug>

#include "importtabledialog.h"
#include "importtablelogdialog.h"
#include "database.h"
#include "sqliteprocess.h"


ImportTableDialog::ImportTableDialog(QWidget * parent, const QString & tableName, const QString & schema)
	: QDialog(parent),
	  m_parent(parent),
	  m_tableName(tableName)
{
	setupUi(this);

	QString n;
	int i = 0;
	int currIx = 0;
	foreach (n, Database::getDatabases().keys())
	{
		if (n == schema)
			currIx = i;
		schemaComboBox->addItem(n);
		++i;
	}
	schemaComboBox->setCurrentIndex(currIx);
	setTablesForSchema(schema);

#if QT_VERSION < 0x040300
	tabWidget->setTabEnabled(1, false);
#endif

	connect(schemaComboBox, SIGNAL(currentIndexChanged(const QString &)),
			this, SLOT(setTablesForSchema(const QString &)));
	connect(fileButton, SIGNAL(clicked()), this, SLOT(fileButton_clicked()));
	connect(buttonBox, SIGNAL(accepted()), this, SLOT(slotAccepted()));
	connect(tabWidget, SIGNAL(currentChanged(int)),
			this, SLOT(createPreview(int)));
	// csv
	connect(pipeRadioButton, SIGNAL(clicked(bool)),
			this, SLOT(createPreview(bool)));
	connect(commaRadioButton, SIGNAL(clicked(bool)),
			this, SLOT(createPreview(bool)));
	connect(semicolonRadioButton, SIGNAL(clicked(bool)),
			this, SLOT(createPreview(bool)));
	connect(tabelatorRadioButton, SIGNAL(clicked(bool)),
			this, SLOT(createPreview(bool)));
	connect(customRadioButton, SIGNAL(clicked(bool)),
			this, SLOT(createPreview(bool)));
	connect(customEdit, SIGNAL(textChanged(QString)),
			this, SLOT(customEdit_textChanged(QString)));
}

void ImportTableDialog::fileButton_clicked()
{
	QString pth(fileEdit->text());
	pth = pth.isEmpty() ? QDir::currentPath() : pth;
	QString fname = QFileDialog::getOpenFileName(this, tr("File to Import"),
												 pth,
												 tr("CSV Files (*.csv);;MS Excel XML (*.xml);;Text Files (*.txt);;All Files (*)"));
	if (fname.isEmpty())
		return;

	fileEdit->setText(fname);
	createPreview();
}

void ImportTableDialog::slotAccepted()
{
	QList<QStringList> values;

	if (fileEdit->text().isEmpty())
		return;

	switch (tabWidget->currentIndex())
	{
		case 0:
			if (sqliteSeparator().length() == 0)
			{
				QMessageBox::warning(this, tr("Data Import"),
									tr("Fields separator must be given"));
				return;
			}
			values = ImportTable::CSVModel(fileEdit->text(),
										   sqliteSeparator(),
										   this, 0).m_values;
			break;
		case 1:
			values = ImportTable::XMLModel(fileEdit->text(), this, 0).m_values;
	}

	// base import
	bool result = true;
	QStringList l;
	QStringList log;
	int cols = Database::tableFields(tableComboBox->currentText(),
									 schemaComboBox->currentText()).count();
	int row = 0;
	int success = 0;
	QString sql("insert into %1.%2 values (%3);");
	QSqlQuery query(QSqlDatabase::database(SESSION_NAME));

	QStringList binds;
	for (int i = 0; i < cols; ++i)
		binds << "?";
	sql = sql.arg(schemaComboBox->currentText(),
				  tableComboBox->currentText(),
				  binds.join(", "));

	if (!Database::execSql("BEGIN TRANSACTION;"))
		return;

	foreach (l, values)
	{
		++row;
		if (l.count() != cols)
		{
			log.append(tr("Row = %1; Imported values = %2; Table columns count = %3; Values = (%4)")
					.arg(row).arg(l.count()).arg(cols).arg(l.join(", ")));
			result = false;
			continue;
		}

		query.prepare(sql);
		for (int i = 0; i < cols ; ++i)
			query.addBindValue(l.at(i));

		query.exec();
		if (query.lastError().isValid())
		{
			log.append(tr("Row = %1; %2").arg(row).arg(query.lastError().text()));
			result = false;
		}
		else
			++success;
	}

	if (result)
	{
		Database::execSql("COMMIT;");
		accept();
	}
	else
	{
		ImportTableLogDialog dia(log, this);
		if (dia.exec())
		{
			if (Database::execSql("COMMIT;"))
				accept();
		}
		Database::execSql("ROLLBACK;");
	}
}

QString ImportTableDialog::sqliteSeparator()
{
	if (pipeRadioButton->isChecked())
		return "|";
	else if (commaRadioButton->isChecked())
		return ",";
	else if (semicolonRadioButton->isChecked())
		return ";";
	else if (tabelatorRadioButton->isChecked())
		return "\t";
	return customEdit->text();
}

void ImportTableDialog::createPreview(int)
{
	if (fileEdit->text().isEmpty())
		return;
	switch (tabWidget->currentIndex())
	{
		case 0:
			previewView->setModel(new ImportTable::CSVModel(fileEdit->text(), sqliteSeparator(), this, 3));
			break;
		case 1:
			previewView->setModel(new ImportTable::XMLModel(fileEdit->text(), this, 3));
			break;
	}
}

void ImportTableDialog::createPreview(bool)
{
	createPreview();
}

void ImportTableDialog::customEdit_textChanged(QString)
{
	if (customRadioButton->isChecked())
		createPreview();
}

/*
Models
 */
ImportTable::BaseModel::BaseModel(QObject * parent)
	: QAbstractTableModel(),
	m_columns(0)
{
	m_values.clear();
}

int ImportTable::BaseModel::rowCount(const QModelIndex & /*parent*/) const
{
	return m_values.count();
}

int ImportTable::BaseModel::columnCount(const QModelIndex & /*parent*/) const
{
	return m_columns;
}

QVariant ImportTable::BaseModel::data(const QModelIndex & index, int role) const
{
	if (!index.isValid())
		return QVariant();
	if (role == Qt::DisplayRole || role == Qt::EditRole)
	{
		if (m_values.count() <= index.row())
			return QVariant();
		if (m_values.at(index.row()).count() <= index.column())
			return QVariant();
		return QVariant(m_values.at(index.row()).at(index.column()));
	}
	return QVariant();
}

QVariant ImportTable::BaseModel::headerData(int section, Qt::Orientation orientation, int role) const
{
	if (role != Qt::DisplayRole)
         return QVariant();

	if (orientation == Qt::Horizontal)
		return QString("%1").arg(section + 1);

	return QVariant();
}

ImportTable::CSVModel::CSVModel(QString fileName, QString separator, QObject * parent, int maxRows)
	: BaseModel(parent)
{
	QFile f(fileName);
	if (!f.open(QIODevice::ReadOnly | QIODevice::Text))
	{
		QMessageBox::warning(qobject_cast<QWidget*>(parent), tr("Data Import"),
							 tr("Cannot open file %1 for reading.").arg(fileName));
		return;
	}

	QTextStream in(&f);
	int r = 0;
	QStringList row;
	while (!in.atEnd())
	{
		row = in.readLine().split(separator);
		if (row.count() > m_columns)
			m_columns = row.count();
		m_values.append(row);
		if (r > maxRows)
			break;
		if (maxRows != 0)
			++r;
	}
	f.close();
}

ImportTable::XMLModel::XMLModel(QString fileName, QObject * parent, int maxRows)
	: BaseModel(parent)
{
#if QT_VERSION >= 0x040300
	QFile file(fileName);
	if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
	{
		QMessageBox::warning(qobject_cast<QWidget*>(parent), tr("Data Import"),
							 tr("Cannot open file %1 for reading.").arg(fileName));
		return;
	}

	QXmlStreamReader xml(&file);
	QStringList row;
	bool isCell = false;
	int r = 0;

	while (!xml.atEnd())
	{
		xml.readNext();
		if (xml.isStartElement())
		{
			if (xml.name() == "Row")
			{
				row.clear();
				isCell = false;
			}
			if (xml.name() == "Cell")
				isCell = true;
			if (isCell && xml.name() == "Data")
				row.append(xml.readElementText());
		}
		if (xml.isEndElement())
		{
			if (xml.name() == "Cell")
				isCell = false;
			if (xml.name() == "Row")
			{
				m_values.append(row);
				if (row.count() > m_columns)
					m_columns = row.count();
				row.clear();
				isCell = false;
				if (r > maxRows)
					break;
				if (maxRows != 0)
					++r;
			}
		}
	}
	if (xml.error() && xml.error() != QXmlStreamReader::PrematureEndOfDocumentError)
	{
        qDebug() << "XML ERROR:" << xml.lineNumber() << ": " << xml.errorString();
    }

	file.close();
#endif
}

void ImportTableDialog::setTablesForSchema(const QString & schema)
{
	int currIx = 0;
	int i = 0;
	QString n;

	tableComboBox->clear();
	foreach (n, Database::getObjects("table", schema).keys())
	{
		if (n == m_tableName)
			currIx = i;
		tableComboBox->addItem(n);
		++i;
	}
	tableComboBox->setCurrentIndex(currIx);
}
