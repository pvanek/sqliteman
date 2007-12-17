/*
For general Sqliteman copyright and licensing information please refer
to the COPYING file provided with the program. Following this notice may exist
a copyright and/or license notice that predates the release of Sqliteman
for which a new license (GPL+exception) is in place.
*/

#ifndef IMPORTTABLEDIALOG_H
#define IMPORTTABLEDIALOG_H

#include "ui_importtabledialog.h"


/*! \brief Import data into table using various importer types.
\note XML import requires Qt library at least in the 4.3.0 version.
\author Petr Vanek <petr@scribus.info>
*/
class ImportTableDialog : public QDialog, public Ui::ImportTableDialog
{
	Q_OBJECT

	public:
		ImportTableDialog(QWidget * parent = 0, const QString & tableName = 0, const QString & schema = 0);

	private:
		QObject * m_parent;
		//! Remember the originally requsted name
		QString m_tableName;

		QString sqliteSeparator();

		void sqlitePreview();

	private slots:
		void fileButton_clicked();
		//! \brief Main import is handled here
		void slotAccepted();
		//! \brief Overloaded due the defined Qt signal/slot
		void createPreview(int i = 0);
		//! \brief Overloaded due the defined Qt signal/slot
		void createPreview(bool);
		void customEdit_textChanged(QString);
		//
		void setTablesForSchema(const QString & schema);

};

//! \brief A helper classes used for data import.
namespace ImportTable
{

	/*! \brief A base Model for all import "modules".
	It's a model in qt4 mvc architecture. See Qt4 docs for
	methods meanings.
	\author Petr Vanek <petr@scribus.info>
	*/
	class BaseModel : public QAbstractTableModel
	{
		Q_OBJECT

		public:
			BaseModel(QObject * parent = 0);

			int rowCount(const QModelIndex & parent = QModelIndex()) const;
			int columnCount(const QModelIndex & parent = QModelIndex()) const;

			QVariant data(const QModelIndex & index, int role = Qt::DisplayRole) const;

			QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const;

			//! \brief Maximum columns of all rows in the model. See columnCount();
			int m_columns;
			/*! \brief Internal structure of values.
			It's filled by format parsers in inherited classes */
			QList<QStringList> m_values;
	};


	//! \brief Comma Separated Values importer
	class CSVModel : public BaseModel
	{
		Q_OBJECT

		public:
			CSVModel(QString fileName, QString separator, QObject * parent = 0, int maxRows = 0);
	};

	/*! \brief MS Excel XML importer
	\note XML import requires Qt library at least in the 4.3.0 version.
	*/
	class XMLModel : public BaseModel
	{
		Q_OBJECT

		public:
			XMLModel(QString fileName, QObject * parent = 0, int maxRows = 0);
	};

};

#endif
