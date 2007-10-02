/*
For general Sqliteman copyright and licensing information please refer
to the COPYING file provided with the program. Following this notice may exist
a copyright and/or license notice that predates the release of Sqliteman
for which a new license (GPL+exception) is in place.
*/
#ifndef SQLMODELS_H
#define SQLMODELS_H

#include <QSqlTableModel>
#include <QItemDelegate>
#include <QSqlRecord>

class QPushButton;
class QByteArray;


/*! \brief Simple color/behaviour improvements for standard Qt4 Sql Models */
class SqlTableModel : public QSqlTableModel
{
	Q_OBJECT

	public:
		SqlTableModel( QObject * parent = 0, QSqlDatabase db = QSqlDatabase() );
		~SqlTableModel() {};

		void setSchema(const QString & schema) { m_schema = schema; };
		QString schema() { return m_schema; };

		bool pendingTransaction() { return m_pending; };

		/*! \brief Set the pending flag \see m_pending to the transaction state
		and refresh the QTableView vertical header in the case of rollback.
		The chached rows to delete are stored in the \see m_deleteCache.
		\param pending true in the case of active transaction in progress.
		*/
		void setPendingTransaction(bool pending);
		bool insertRows ( int row, int count, const QModelIndex & parent = QModelIndex() );
		bool removeRows ( int row, int count, const QModelIndex & parent = QModelIndex() );

	private:
		bool m_useNull;
		QColor m_nullColor;
		QString m_nullText;
		bool m_useBlob;
		QColor m_blobColor;
		QString m_blobText;
		bool m_pending;
		QString m_schema;
		QList<int> m_deleteCache;
		bool m_cropColumns;

		QVariant data(const QModelIndex & item, int role = Qt::DisplayRole) const;
		bool setData(const QModelIndex & index, const QVariant & value, int role = Qt::EditRole);

	private slots:
		//! \brief Called when is new row created in the view (not in the model).
		void doPrimeInsert(int, QSqlRecord &);
};

/*! \brief Simple color/behaviour improvements for standard Qt4 Sql Models */
class SqlQueryModel : public QSqlQueryModel
{
	Q_OBJECT

	public:
		SqlQueryModel( QObject * parent = 0);
		~SqlQueryModel() {};
		void setQuery ( const QSqlQuery & query );
		void setQuery ( const QString & query, const QSqlDatabase & db = QSqlDatabase() );
		bool pendingTransaction() { return false; };

	private:
		bool m_useNull;
		QColor m_nullColor;
		QString m_nullText;
		bool m_useBlob;
		QColor m_blobColor;
		QString m_blobText;
		QSqlRecord info;
		bool m_cropColumns;

		QVariant data(const QModelIndex & item, int role = Qt::DisplayRole) const;
};

#endif
