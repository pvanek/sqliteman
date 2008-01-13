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

class DataViewer;
class QProgressDialog;
class QSqlQueryModel;


/*! \brief GUI for data export into file or clipboard
\author Petr Vanek <petr@scribus.info>
*/
class DataExportDialog : public QDialog
{
		Q_OBJECT
	public:
		DataExportDialog(DataViewer * parent = 0, const QString & tableName = 0);
		~DataExportDialog(){};

		bool doExport();

	private:
		const QString m_tableName;
		bool cancelled;
		QSqlQueryModel * m_data;
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

		//! \brief Enable or Disable "OK" button depending on the GUI options
		void checkButtonStatus();

	private slots:
		void fileButton_toggled(bool);
		void clipboardButton_toggled(bool);
		void fileEdit_textChanged(const QString &);
		void searchButton_clicked();
		void cancel();
		void slotAccepted();
};

#endif
