/*
For general Sqliteman copyright and licensing information please refer
to the COPYING file provided with the program. Following this notice may exist
a copyright and/or license notice that predates the release of Sqliteman
for which a new license (GPL+exception) is in place.
*/

#include <QColor>
#include <QSqlField>

#include "sqlmodels.h"
#include "database.h"
#include "preferences.h"


SqlTableModel::SqlTableModel(QObject * parent, QSqlDatabase db)
	: QSqlTableModel(parent, db),
	m_pending(false),
	m_schema("")
{
	m_deleteCache.clear();
	Preferences * prefs = Preferences::instance();
	m_useNull = prefs->nullHighlight();
	m_nullColor = prefs->nullHighlightColor();
	m_nullText = prefs->nullHighlightText();
	m_useBlob = prefs->blobHighlight();
	m_blobColor = prefs->blobHighlightColor();
	m_blobText = prefs->blobHighlightText();
	m_cropColumns = prefs->cropColumns();

	connect(this, SIGNAL(primeInsert(int, QSqlRecord &)),
			this, SLOT(doPrimeInsert(int, QSqlRecord &)));
}

QVariant SqlTableModel::data(const QModelIndex & item, int role) const
{
	QString curr(QSqlTableModel::data(item, Qt::DisplayRole).toString());
	// numbers
	if (role == Qt::TextAlignmentRole)
	{
		bool ok;
		curr.toDouble(&ok);
		if (ok)
			return QVariant(Qt::AlignRight | Qt::AlignTop);
		return QVariant(Qt::AlignTop);
	}

	// mark rows prepared for a deletion in this trasnaction
	if (role == Qt::BackgroundColorRole && m_deleteCache.contains(item.row()))
		return QVariant(Qt::red);

	// nulls
	if (m_useNull && curr.isNull())
	{
		if (role == Qt::BackgroundColorRole)
			return QVariant(m_nullColor);
		if (role == Qt::ToolTipRole)
			return QVariant(tr("NULL value"));
		if (role == Qt::DisplayRole)
			return QVariant(m_nullText);
	}
	// BLOBs
	// any others handling with blobs - e.g. converting to images etc.
	// are followed with serious perfromance issues.
	// Users can see it through edit dialog.
	if (/*f.type.toUpper() == "BLOB" || */
		m_useBlob /*&&
		   record().field(item.column()).type() == QVariant::ByteArray*/
		   && QSqlTableModel::data(item, Qt::DisplayRole).type() == QVariant::ByteArray)
	{
		if (role == Qt::BackgroundColorRole)
			return QVariant(m_blobColor);
		if (role == Qt::ToolTipRole)
			return QVariant(tr("BLOB value"));
		if (role == Qt::DisplayRole)
			return QVariant(m_blobText);
		if (role == Qt::EditRole)
// 			return Database::hex(QSqlTableModel::data(item, Qt::DisplayRole).toByteArray());
			return QSqlTableModel::data(item, Qt::DisplayRole);
	}

	// advanced tooltips
	if (role == Qt::ToolTipRole)
		return QVariant("<qt>" + curr + "</qt>");

	if (role == Qt::DisplayRole && m_cropColumns)
		return QVariant(curr.length() > 20 ? curr.left(20)+"..." : curr);

	return QSqlTableModel::data(item, role);
}

bool SqlTableModel::setData ( const QModelIndex & index, const QVariant & value, int role)
{
	if (role == Qt::EditRole)
		m_pending = true;
	return QSqlTableModel::setData(index, value, role);
}

void SqlTableModel::doPrimeInsert(int row, QSqlRecord & record)
{
	FieldList fl = Database::tableFields(tableName(), m_schema);
	bool ok;
	QString defval;
	// guess what type is the default value.
	foreach (DatabaseTableField column, fl)
	{
		if (column.defval.isNull())
			continue;
		defval = column.defval;
		defval.toInt(&ok);
		if (!ok)
		{
			defval.toDouble(&ok);
			if (!ok)
			{
				if (defval.left(1) == "'" || defval.left(1) == "\"")
					defval = defval.mid(1, defval.length()-2);
			}
		}
		record.setValue(column.name, QVariant(defval));
	}
}

bool SqlTableModel::insertRows ( int row, int count, const QModelIndex & parent)
{
	m_pending = true;
	return QSqlTableModel::insertRows(row, count, parent);
}

bool SqlTableModel::removeRows ( int row, int count, const QModelIndex & parent)
{
	m_pending = true;
	// this is a workaround to allow mark heading as deletion
	// (as it's propably a bug in Qt QSqlTableModel ManualSubmit handling
	bool ret = QSqlTableModel::removeRows(row, count, parent);
	emit dataChanged( index(row, 0), index(row+count-1, columnCount()-1) );
	emit headerDataChanged(Qt::Vertical, row, row+count-1);
	for (int i = 0; i < count; ++i)
		m_deleteCache.append(row+i);

	return ret;
}

void SqlTableModel::setPendingTransaction(bool pending)
{
	m_pending = pending;

	// TODO: examine the better way to get only shown/changed lines.
	// If there is one...
	if (!pending)
	{
		for (int i = 0; i < m_deleteCache.size(); ++i)
			emit headerDataChanged(Qt::Vertical, m_deleteCache[i], m_deleteCache[i]);
	}
	m_deleteCache.clear();
}




SqlQueryModel::SqlQueryModel( QObject * parent)
	: QSqlQueryModel(parent)
{
	Preferences * prefs = Preferences::instance();
	m_useNull = prefs->nullHighlight();
	m_nullColor = prefs->nullHighlightColor();
	m_nullText = prefs->nullHighlightText();
	m_useBlob = prefs->blobHighlight();
	m_blobColor = prefs->blobHighlightColor();
	m_blobText = prefs->blobHighlightText();
	m_cropColumns = prefs->cropColumns();
}

QVariant SqlQueryModel::data(const QModelIndex & item, int role) const
{
	QString curr(QSqlQueryModel::data(item, Qt::DisplayRole).toString());

	// numbers
	if (role == Qt::TextAlignmentRole)
	{
		bool ok;
		curr.toDouble(&ok);
		if (ok)
			return QVariant(Qt::AlignRight | Qt::AlignTop);
		return QVariant(Qt::AlignTop);
	}

	if (m_useNull && curr.isNull())
	{
		if (role == Qt::BackgroundColorRole)
			return QVariant(m_nullColor);
		if (role == Qt::ToolTipRole)
			return QVariant(tr("NULL value"));
		if (role == Qt::DisplayRole)
			return QVariant(m_nullText);
	}

	if (/*f.type.toUpper() == "BLOB" || */
		m_useBlob /*&&
		   record().field(item.column()).type() == QVariant::ByteArray*/
		   && QSqlQueryModel::data(item, Qt::DisplayRole).type() == QVariant::ByteArray)
	{
		if (role == Qt::BackgroundColorRole)
			return QVariant(m_blobColor);
		if (role == Qt::ToolTipRole)
			return QVariant(tr("BLOB value"));
		if (role == Qt::DisplayRole)
			return QVariant(m_blobText);
	}

	// advanced tooltips
	if (role == Qt::ToolTipRole)
		return QVariant("<qt>" + curr + "</qt>");

	if (role == Qt::DisplayRole && m_cropColumns)
		return QVariant(curr.length() > 20 ? curr.left(20)+"..." : curr);

	return QSqlQueryModel::data(item, role);
}

void SqlQueryModel::setQuery ( const QSqlQuery & query )
{
	QSqlQueryModel::setQuery(query);
	info = record();
}

void SqlQueryModel::setQuery ( const QString & query, const QSqlDatabase & db)
{
	QSqlQueryModel::setQuery(query, db);
	info = record();
}
