/*
For general Sqliteman copyright and licensing information please refer
to the COPYING file provided with the program. Following this notice may exist
a copyright and/or license notice that predates the release of Sqliteman
for which a new license (GPL+exception) is in place.
*/
#include <QFileDialog>
#include <QMessageBox>
#include <QXmlStreamReader>

#include "importtabledialog.h"
#include "database.h"
#include "sqliteprocess.h"

#include <QtDebug>

ImportTableDialog::ImportTableDialog(QWidget * parent, const QString & tableName, const QString & schema)
	: QDialog(parent),
	  m_parent(parent)
{
	setupUi(this);

	QString n;
	int i = 0;
	int currIx = 0;
	foreach (n, Database::getObjects("table", schema).keys())
	{
		if (n == tableName)
			currIx = i;
		tableComboBox->addItem(n);
		++i;
	}
	tableComboBox->setCurrentIndex(currIx);

	connect(fileButton, SIGNAL(clicked()), this, SLOT(fileButton_clicked()));
	connect(buttonBox, SIGNAL(accepted()), this, SLOT(slotAccepted()));
	connect(tabWidget, SIGNAL(currentChanged(int)), this, SLOT(createPreview(int)));
	// csv
	connect(pipeRadioButton, SIGNAL(clicked(bool)), this, SLOT(createPreview(bool)));
	connect(commaRadioButton, SIGNAL(clicked(bool)), this, SLOT(createPreview(bool)));
	connect(semicolonRadioButton, SIGNAL(clicked(bool)), this, SLOT(createPreview(bool)));
	connect(tabelatorRadioButton, SIGNAL(clicked(bool)), this, SLOT(createPreview(bool)));
	connect(customRadioButton, SIGNAL(clicked(bool)), this, SLOT(createPreview(bool)));
	connect(customEdit, SIGNAL(textChanged(QString)), this, SLOT(customEdit_textChanged(QString)));
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
	bool result = false;

	switch (tabWidget->currentIndex())
	{
		case 0:
			result = sqliteImport();
			break;
		// TODO: more stuff
	}
	if (result)
		accept();
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
		return "\"\\t\"";
	return customEdit->text();
}

bool ImportTableDialog::sqliteImport()
{
	QString out;
	if (sqliteSeparator().length() == 0)
	{
		QMessageBox::warning(this, tr("Data Import"),
							 tr("Fields separator must be given"));
		return false;
	}
	SqliteProcess imp(m_parent);
	imp.start(QStringList() << ".separator " << sqliteSeparator() << " .import " << fileEdit->text() << " " << tableComboBox->currentText());
	qDebug() << imp.errorMessage();
	qDebug() << imp.success();
	qDebug() << imp.allStderr();
	return false;
}

void ImportTableDialog::createPreview(int)
{
	switch (tabWidget->currentIndex())
	{
		case 0:
			previewView->setModel(new ImportTable::CSVModel(fileEdit->text(), sqliteSeparator(), this));
			break;
		case 1:
			previewView->setModel(new ImportTable::XMLModel(fileEdit->text(), this));
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
		if (++r > maxRows)
			break;
	}
	f.close();
}

ImportTable::XMLModel::XMLModel(QString fileName, QObject * parent, int maxRows)
	: BaseModel(parent)
{
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
			}
		}
	}
	if (xml.error() && xml.error() != QXmlStreamReader::PrematureEndOfDocumentError)
	{
        qDebug() << "XML ERROR:" << xml.lineNumber() << ": " << xml.errorString();
    }

	file.close();
}
