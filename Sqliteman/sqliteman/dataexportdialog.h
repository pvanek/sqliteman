/*
For general Sqliteman copyright and licensing information please refer
to the COPYING file provided with the program. Following this notice may exist
a copyright and/or license notice that predates the release of Sqliteman
for which a new license (GPL+exception) is in place.
*/

#ifndef DATAEXPORTDIALOG_H
#define DATAEXPORTDIALOG_H

#include <QDialog>
#include <QTextStream>
#include <QSqlTableModel>
#include <QFile>

#include "ui_dataexportdialog.h"
#include "sqlmodels.h"

class DataViewer;
class QProgressDialog;


/*! \brief GUI for data export into file or clipboard
\author Petr Vanek <petr@scribus.info>
*/
class DataExportDialog : public QDialog
{
		Q_OBJECT
	public:
		DataExportDialog(DataViewer * parent = 0);
		~DataExportDialog(){};

		bool doExport();

	private:
		bool cancelled;
		SqlTableModel * m_data;
		QStringList m_header;
		QProgressDialog * progress;

		QTextStream out;
		QString clipboard;
		QFile file;
		bool exportFile;

		Ui::DataExportDialog ui;
		QMap<QString,QString> formats;

		bool exportCSV();
		bool exportHTML();
		bool exportExcelXML();
		bool exportSql();
		bool exportPython();

		bool openStream();
		bool closeStream();

		bool setProgress(int p);

		/*! \brief Export table header strings too?
		\retval bool true = export, false = do not export header */
		bool header();
		QString endl();

	private slots:
		void fileButton_toggled(bool);
		void searchButton_clicked();
		void cancel();
};

#endif
