/*
For general Sqliteman copyright and licensing information please refer
to the COPYING file provided with the program. Following this notice may exist
a copyright and/or license notice that predates the release of Sqliteman
for which a new license (GPL+exception) is in place.
*/

#ifndef IMPORTTABLEDIALOG_H
#define IMPORTTABLEDIALOG_H

#include "ui_importtabledialog.h"


/*! \brief Import data into table using .import
\author Petr Vanek <petr@scribus.info>
*/
class ImportTableDialog : public QDialog, public Ui::ImportTableDialog
{
	Q_OBJECT

	public:
		ImportTableDialog(QWidget * parent = 0, const QString & tableName = 0, const QString & schema = 0);

	private:
		QObject * m_parent;
		bool sqliteImport();
		QString sqliteSeparator();

		void sqlitePreview();

	private slots:
		void fileButton_clicked();
		void slotAccepted();
		void createPreview(int i = 0);
		void createPreview(bool);
		void customEdit_textChanged(QString);

};

namespace ImportTable
{

	class CSVModel : public QAbstractTableModel
	{
		Q_OBJECT

		public:
			CSVModel(QString fileName, QString separator, QObject * parent = 0, int maxRows = 5);

			int rowCount(const QModelIndex & parent = QModelIndex()) const;
			int columnCount(const QModelIndex & parent = QModelIndex()) const;

			QVariant data(const QModelIndex & index, int role = Qt::DisplayRole) const;

			QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const;

		private:
			int m_columns;
			QList<QStringList> m_values;
	};

};

#endif
