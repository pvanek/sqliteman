/*
For general Sqliteman copyright and licensing information please refer
to the COPYING file provided with the program. Following this notice may exist
a copyright and/or license notice that predates the release of Sqliteman
for which a new license (GPL+exception) is in place.
*/

#ifndef TABLETREE_H
#define TABLETREE_H

#include <QTreeWidget>


/*! \brief Schema browser.
A tree structure containing sorted database objects.
\author Petr Vanek <petr@scribus.info>
*/
class TableTree : public QTreeWidget
{
		Q_OBJECT
	public:
		static const int TablesItemType = QTreeWidgetItem::UserType;
		static const int ViewsItemType = QTreeWidgetItem::UserType + 1;
		static const int TableType = QTreeWidgetItem::UserType + 2;
		static const int ViewType = QTreeWidgetItem::UserType + 3;
		static const int IndexesItemType = QTreeWidgetItem::UserType + 4;
		static const int IndexType = QTreeWidgetItem::UserType + 5;
		static const int TriggersItemType = QTreeWidgetItem::UserType + 6;
		static const int TriggerType = QTreeWidgetItem::UserType + 7;
		static const int SystemItemType = QTreeWidgetItem::UserType + 8;
		static const int SystemType = QTreeWidgetItem::UserType + 9;
		static const int DatabaseItemType = QTreeWidgetItem::UserType + 10;
		static const int SysIndexesItemType = QTreeWidgetItem::UserType + 11;
		static const int SysIndexType = QTreeWidgetItem::UserType + 12;
		static const int ColumnType = QTreeWidgetItem::UserType + 13;
		static const int ColumnItemType = QTreeWidgetItem::UserType + 14;

		TableTree(QWidget * parent = 0);
		~TableTree(){};

		void buildDatabase(QTreeWidgetItem * dbItem, const QString & schema);
		void buildDatabase(const QString & schema);
		void buildTables(QTreeWidgetItem * tablesItem, const QString & schema);
		void buildIndexes(QTreeWidgetItem *indexesItem, const QString & schema, const QString & table);
		void buildColumns(QTreeWidgetItem * columnsItem, const QString & schema, const QString & table);
		void buildSysIndexes(QTreeWidgetItem *indexesItem, const QString & schema, const QString & table);
		void buildTriggers(QTreeWidgetItem *triggersItem, const QString & schema, const QString & table);
		void buildViews(QTreeWidgetItem * viewsItem, const QString & schema);
		void buildCatalogue(QTreeWidgetItem * systemItem, const QString & schema);

		QString trDatabase;
		QString trTables;
		QString trIndexes;
		QString trSysIndexes;
		QString trViews;
		QString trTriggers;
		QString trSys;
		QString trCols;

		QList<QTreeWidgetItem*> searchMask(const QString & trStr);

	public slots:
		void buildTree();
		void buildViewTree(QString schema, QString name);

	private:
		void deleteChildren(QTreeWidgetItem * item);
		QString trLabel(const QString & trStr);

		QPoint m_dragStartPosition;

		void mousePressEvent(QMouseEvent *event);
		void mouseMoveEvent(QMouseEvent *event);
};

#endif
