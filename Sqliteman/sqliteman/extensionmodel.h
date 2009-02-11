/*
For general Sqliteman copyright and licensing information please refer
to the COPYING file provided with the program. Following this notice may exist
a copyright and/or license notice that predates the release of Sqliteman
for which a new license (GPL+exception) is in place.
*/
#ifndef EXTENSIONMODEL_H
#define EXTENSIONMODEL_H

#include <QAbstractTableModel>
#include <QStringList>


/*! \brief A model for extensions dipslay.
Extensions are stored as a files with full path in m_values.
\author Petr Vanek <petr@scribus.info>
*/
class ExtensionModel : public QAbstractTableModel
{
	Q_OBJECT

	public:
		ExtensionModel(QObject * parent = 0);
		~ExtensionModel();

		int rowCount(const QModelIndex & parent = QModelIndex()) const;
		int columnCount(const QModelIndex & parent = QModelIndex()) const;

		QVariant data(const QModelIndex & index, int role = Qt::DisplayRole) const;

		Qt::ItemFlags flags(const QModelIndex & index) const;

		QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const;

		void setExtensions(const QStringList & l);
		QStringList extensions() { return m_values; };

	private:
		QStringList m_values;
};

#endif
