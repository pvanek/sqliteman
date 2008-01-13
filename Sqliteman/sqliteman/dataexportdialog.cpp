/*
For general Sqliteman copyright and licensing information please refer
to the COPYING file provided with the program. Following this notice may exist
a copyright and/or license notice that predates the release of Sqliteman
for which a new license (GPL+exception) is in place.
*/

#include <QMessageBox>
#include <QClipboard>
#include <QCheckBox>
#include <QFile>
#include <QSqlRecord>
#include <QFileDialog>
#include <QDir>
#include <QProgressDialog>
#include <QTextCodec>
#include <QSqlQueryModel>

#include "dataviewer.h"
#include "dataexportdialog.h"
#include "database.h"
#include "preferences.h"

#define LF QChar(0x0A)  /* '\n' */
#define CR QChar(0x0D)  /* '\r' */


DataExportDialog::DataExportDialog(DataViewer * parent, const QString & tableName) :
		QDialog(0),
		m_tableName(tableName),
		file(0)
{
	Preferences * prefs = Preferences::instance();

	m_data = parent->tableData();
	m_header = parent->tableHeader();
	cancelled = false;

	ui.setupUi(this);
	formats[tr("Comma Separated Values (CSV)")] = "csv";
	formats[tr("HTML")] = "html";
	formats[tr("MS Excel XML (XLS)")] = "xls";
	formats[tr("SQL inserts")] = "sql";
	formats[tr("Python List")] = "py";
	ui.formatBox->addItems(formats.keys());
	ui.formatBox->setCurrentIndex(prefs->exportFormat());

	ui.lineEndBox->addItem("UNIX (lf)");
	ui.lineEndBox->addItem("Macintosh (cr)");
	ui.lineEndBox->addItem("MS Windows (crlf)");
	ui.lineEndBox->setCurrentIndex(prefs->exportEol());

	QStringList enc;
	foreach (QString s, QTextCodec::availableCodecs())
		enc << s;
	enc.sort();
	ui.encodingBox->addItems(enc);
	ui.encodingBox->setCurrentIndex(enc.indexOf(prefs->exportEncoding()));

	ui.fileButton->setChecked(prefs->exportDestination() == 0);
	ui.clipboardButton->setChecked(prefs->exportDestination() == 1);
	ui.headerCheckBox->setChecked(prefs->exportHeaders());

	checkButtonStatus();

	connect(ui.fileButton, SIGNAL(toggled(bool)),
			this, SLOT(fileButton_toggled(bool)));
	connect(ui.clipboardButton, SIGNAL(toggled(bool)),
			this, SLOT(clipboardButton_toggled(bool)));
	connect(ui.fileEdit, SIGNAL(textChanged(const QString &)),
			this, SLOT(fileEdit_textChanged(const QString &)));
	connect(ui.searchButton, SIGNAL(clicked()),
			this, SLOT(searchButton_clicked()));
	connect(ui.buttonBox, SIGNAL(accepted()),
			this, SLOT(slotAccepted()));
}

void DataExportDialog::slotAccepted()
{
	Preferences * prefs = Preferences::instance();
	prefs->setExportFormat(ui.formatBox->currentIndex());
	prefs->setExportDestination(ui.fileButton->isChecked() ? 0 : 1);
	prefs->setExportHeaders(ui.headerCheckBox->isChecked());
	prefs->setExportEncoding(ui.encodingBox->currentText());
	prefs->setExportEol(ui.lineEndBox->currentIndex());

	accept();
}

void DataExportDialog::checkButtonStatus()
{
	bool e = false;
	// NULL
	if (ui.fileButton->isChecked() && !ui.fileEdit->text().isEmpty())
		e = true;
	if (ui.clipboardButton->isChecked())
		e = true;
	ui.buttonBox->button(QDialogButtonBox::Ok)->setEnabled(e);
}

bool DataExportDialog::doExport()
{
	progress = new QProgressDialog("Exporting...", "Abort", 0, 0, this);
	connect(progress, SIGNAL(canceled()), this, SLOT(cancel()));
	progress->setWindowModality(Qt::WindowModal);
	// export everything
	while (m_data->canFetchMore())
		m_data->fetchMore();

	progress->setMaximum(m_data->rowCount());

	QString curr(formats[ui.formatBox->currentText()]);
	bool res = openStream();
	if (curr == "csv")
		res &= exportCSV();
	if (curr == "html")
		res &= exportHTML();
	if (curr == "xls")
		res &= exportExcelXML();
	if (curr == "sql")
		res &= exportSql();
	if (curr == "py")
		res &= exportPython();

	if (res)
		res &= closeStream();

	progress->setValue(m_data->rowCount());
	delete progress;
	progress = 0;

	return res;
}

void DataExportDialog::cancel()
{
	cancelled = true;
}

bool DataExportDialog::setProgress(int p)
{
	if (cancelled)
		return false;
	progress->setValue(p);
	qApp->processEvents();
	return true;
}

bool DataExportDialog::openStream()
{
	// file
	exportFile = ui.fileButton->isChecked();
	if (exportFile)
	{
		file.setFileName(ui.fileEdit->text());
		if (!file.open(QFile::WriteOnly | QFile::Truncate))
		{
			QMessageBox::warning(this, tr("Export Error"),
								 tr("Cannot open file %1 for writting").arg(ui.fileEdit->text()));
			return false;
		}
		out.setDevice(&file);
		out.setCodec(QTextCodec::codecForName(ui.encodingBox->currentText().toLatin1()));
	}
	else
	{
		// clipboard
		clipboard = QString();
		out.setString(&clipboard);
	}
	return true;
}

bool DataExportDialog::closeStream()
{
	out.flush();
	if (exportFile)
		file.close();
	else
	{
		QClipboard *c = QApplication::clipboard();
		c->setText(clipboard);
	}
	return true;
}

bool DataExportDialog::exportCSV()
{
	if (header())
	{
		for (int i = 0; i < m_header.size(); ++i)
		{
			out << '"' << m_header.at(i) << '"';
			if (i != (m_header.size() - 1))
				out << ", ";
		}
		out << endl();
	}

	for (int i = 0; i < m_data->rowCount(); ++i)
	{
		if (!setProgress(i))
			return false;
		QSqlRecord r = m_data->record(i);
		for (int j = 0; j < m_header.size(); ++j)
		{
			out << '"' << r.value(j).toString().replace('"', "\"\"").replace('\n', "\\n") << '"';
			if (j != (m_header.size() - 1))
				out << ", ";
		}
		out << endl();
	}
	return true;
}

bool DataExportDialog::exportHTML()
{
	out << "<html>" << endl() << "<header>" << endl();
	out << "<title>Sqliteman export</title>" << endl() << "</header>" << endl();
	out << "<body>" << endl() << "<table border=\"1\">" << endl();

	if (header())
	{
		out << "<tr>";
		for (int i = 0; i < m_header.size(); ++i)
			out << "<th>" << m_header.at(i) << "</th>";
		out << "</tr>" << endl();
	}

	for (int i = 0; i < m_data->rowCount(); ++i)
	{
		if (!setProgress(i))
			return false;
		out << "<tr>";
		QSqlRecord r = m_data->record(i);
		for (int j = 0; j < m_header.size(); ++j)
			out << "<td>" << r.value(j).toString() << "</td>";
		out << "</tr>" << endl();
	}
	out << "</table>" << endl() << "</body>" << endl() << "</html>";
	return true;
}

bool DataExportDialog::exportExcelXML()
{
	out << "<?xml version=\"1.0\"?>" << endl()
		<< "<ss:Workbook xmlns:ss=\"urn:schemas-microsoft-com:office:spreadsheet\">" << endl()
		<< "<ss:Styles><ss:Style ss:ID=\"1\"><ss:Font ss:Bold=\"1\"/></ss:Style></ss:Styles>" << endl()
		<< "<ss:Worksheet ss:Name=\"Sqliteman Export\">" << endl()
		<< "<ss:Table>"<< endl();

	for (int i = 0; i < m_header.size(); ++i)
		out << "<ss:Column ss:Width=\"100\"/>" << endl();

	if (header())
	{
		out << "<ss:Row ss:StyleID=\"1\">" << endl();
		for (int i = 0; i < m_header.size(); ++i)
			out << "<ss:Cell><ss:Data ss:Type=\"String\">" << m_header.at(i) << "</ss:Data></ss:Cell>" << endl();
		out << "</ss:Row>" << endl();
	}

	for (int i = 0; i < m_data->rowCount(); ++i)
	{
		if (!setProgress(i))
			return false;
		out << "<ss:Row>" << endl();
		QSqlRecord r = m_data->record(i);
		for (int j = 0; j < m_header.size(); ++j)
			out << "<ss:Cell><ss:Data ss:Type=\"String\">" << r.value(j).toString() << "</ss:Data></ss:Cell>" << endl();
		out << "</ss:Row>" << endl();
	}

	out << "</ss:Table>" << endl()
		<< "</ss:Worksheet>" << endl()
		<< "</ss:Workbook>" << endl();
	return true;
}

bool DataExportDialog::exportSql()
{
	out << "BEGIN TRANSACTION;" << endl();;
	QString columns(m_header.join("\", \""));

	for (int i = 0; i < m_data->rowCount(); ++i)
	{
		if (!setProgress(i))
			return false;
		out << "insert into " << m_tableName << " (\"" << columns << "\") values (";
		QSqlRecord r = m_data->record(i);

		for (int j = 0; j < m_header.size(); ++j)
		{
			if (r.value(j).toString().isNull())
				out << "NULL";
			else if (r.value(j).type() == QVariant::ByteArray)
				out << Database::hex(r.value(j).toByteArray());
			else
				out << "'" << r.value(j).toString().replace('\'', "''") << "'";
			if (j != (m_header.size() - 1))
				out << ", ";
		}
		out << ");" << endl();;
	}
	out << "COMMIT;" << endl();;
	return true;
}

bool DataExportDialog::exportPython()
{
	out << "[" << endl();

	for (int i = 0; i < m_data->rowCount(); ++i)
	{
		if (!setProgress(i))
			return false;
		out << "    { ";
		QSqlRecord r = m_data->record(i);
		for (int j = 0; j < m_header.size(); ++j)
		{
			// "key" : """value""" python syntax due the potentional EOLs in the strings
			out << "\"" << m_header.at(j) << "\" : \"\"\"" << r.value(j).toString() << "\"\"\"";
			if (j != (m_header.size() - 1))
				out << ", ";
		}
		out << " }," << endl();
	}
	out << "]" << endl();
	return true;
}

void DataExportDialog::fileButton_toggled(bool state)
{
	ui.fileEdit->setEnabled(state);
	ui.searchButton->setEnabled(state);
	ui.label_2->setEnabled(state);
	checkButtonStatus();
}

void DataExportDialog::clipboardButton_toggled(bool)
{
	checkButtonStatus();
}

void DataExportDialog::fileEdit_textChanged(const QString &)
{
	checkButtonStatus();
}

void DataExportDialog::searchButton_clicked()
{
	QString mask;
	QString curr(formats[ui.formatBox->currentText()]);
	if (curr == "csv")
		mask = tr("Comma Separated Value (*.csv)");
	if (curr == "html")
		mask = tr("HTML (*.html)");
	if (curr == "xls")
		mask = tr("MS Excel XML (*.xml)");
	if (curr == "sql")
		mask = tr("SQL inserts (*.sql)");
	if (curr == "py")
		mask = tr("Python list (*.py)");

	QString fileName = QFileDialog::getSaveFileName(this,
			tr("Export to File"),
			QDir::homePath(),
			mask);
	if (!fileName.isNull())
		ui.fileEdit->setText(fileName);
}

bool DataExportDialog::header()
{
	return (ui.headerCheckBox->checkState() == Qt::Checked);
}

QString DataExportDialog::endl()
{
	int ix = ui.lineEndBox->currentIndex();
	switch (ix)
	{
		case 1: return CR;
		case 2: return QString(CR) + LF;
		case 0:
		default:
			return LF;
	}
}
