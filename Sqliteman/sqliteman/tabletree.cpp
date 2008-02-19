/*
For general Sqliteman copyright and licensing information please refer
to the COPYING file provided with the program. Following this notice may exist
a copyright and/or license notice that predates the release of Sqliteman
for which a new license (GPL+exception) is in place.
*/
#include <QMouseEvent>
#include <QApplication>

#include "database.h"
#include "tabletree.h"
#include "utils.h"


TableTree::TableTree(QWidget * parent) : QTreeWidget(parent)
{
	trDatabase = tr("Database");
	trTables = tr("Tables");
	trIndexes = tr("Indexes");
	trSysIndexes = tr("System Indexes");
	trViews = tr("Views");
	trTriggers = tr("Triggers");
	trSys = tr("System Catalogue");
	trCols = tr("Columns");
	
	setColumnCount(2);
	setHeaderLabels(QStringList() << trDatabase << "schema");
	hideColumn(1);
	
	setContextMenuPolicy(Qt::CustomContextMenu);
	setDragDropMode(QAbstractItemView::DragOnly);
	setDragEnabled(true);
	setDropIndicatorShown(true);
	setAcceptDrops(false);
}

void TableTree::buildTree()
{
	QStringList databases(Database::getDatabases().keys());
	clear();

	foreach(QString schema, databases)
	{
		buildDatabase(schema);
	}
}

void TableTree::buildDatabase(QTreeWidgetItem * dbItem, const QString & schema)
{
	deleteChildren(dbItem);
	buildDatabase(schema);
}

void TableTree::buildDatabase(const QString & schema)
{
	QTreeWidgetItem * dbItem = new QTreeWidgetItem(this, DatabaseItemType);
	dbItem->setIcon(0, Utils::getIcon("database.png"));
	dbItem->setText(0, schema);
	dbItem->setText(1, schema);

	QTreeWidgetItem * tablesItem = new QTreeWidgetItem(dbItem, TablesItemType);
	tablesItem->setIcon(0, Utils::getIcon("table.png"));

	QTreeWidgetItem * viewsItem = new QTreeWidgetItem(dbItem, ViewsItemType);
	viewsItem->setIcon(0, Utils::getIcon("view.png"));

	QTreeWidgetItem * systemItem = new QTreeWidgetItem(dbItem, SystemItemType);
	systemItem->setIcon(0, Utils::getIcon("system.png"));

	buildTables(tablesItem, schema);
	buildViews(viewsItem, schema);
	buildCatalogue(systemItem, schema);

	dbItem->setExpanded(true);
}

void TableTree::buildTables(QTreeWidgetItem * tablesItem, const QString & schema)
{
	deleteChildren(tablesItem);

	QStringList tables = Database::getObjects("table", schema).keys();
	tablesItem->setText(0, trLabel(trTables).arg(tables.size()));
	tablesItem->setText(1, schema);

	foreach(QString table, tables)
	{
		QTreeWidgetItem * tableItem = new QTreeWidgetItem(tablesItem, TableType);
		tableItem->setText(0, table);
		tableItem->setText(1, schema);
		// columns
		QTreeWidgetItem *columnsItem = new QTreeWidgetItem(tableItem, ColumnItemType);
		buildColumns(columnsItem, schema, table);
		// indexes
		QTreeWidgetItem *indexesItem = new QTreeWidgetItem(tableItem, IndexesItemType);
		buildIndexes(indexesItem, schema, table);
		// system indexes (unique)
		QTreeWidgetItem *sysIndexesItem = new QTreeWidgetItem(tableItem, SysIndexesItemType);
		buildSysIndexes(sysIndexesItem, schema, table);
		// triggers
		QTreeWidgetItem *triggersItem = new QTreeWidgetItem(tableItem, TriggersItemType);
		buildTriggers(triggersItem, schema, table);
	}
}

void TableTree::buildIndexes(QTreeWidgetItem *indexesItem, const QString & schema, const QString & table)
{
	deleteChildren(indexesItem);
	QStringList values = Database::getObjects("index", schema).values(table);
	indexesItem->setText(0, trLabel(trIndexes).arg(values.size()));
	indexesItem->setIcon(0, Utils::getIcon("index.png"));
	indexesItem->setText(1, schema);
	for (int i = 0; i < values.size(); ++i)
	{
		QTreeWidgetItem *indexItem = new QTreeWidgetItem(indexesItem, IndexType);
		indexItem->setText(0, values.at(i));
		indexItem->setText(1, schema);
	}
}

void TableTree::buildColumns(QTreeWidgetItem * columnsItem, const QString & schema, const QString & table)
{
	deleteChildren(columnsItem);
	FieldList values = Database::tableFields(table, schema);
	columnsItem->setText(0, trLabel(trCols).arg(values.size()));
	columnsItem->setIcon(0, Utils::getIcon("column.png"));
// 	columnsItem->setText(1, schema);
	for (int i = 0; i < values.size(); ++i)
	{
		QTreeWidgetItem *indexItem = new QTreeWidgetItem(columnsItem, ColumnType);
		indexItem->setText(0, values.at(i).name);
// 		indexItem->setText(1, schema);
	}
}

void TableTree::buildSysIndexes(QTreeWidgetItem *indexesItem, const QString & schema, const QString & table)
{
	deleteChildren(indexesItem);
	QStringList sysIx = Database::getSysIndexes(table, schema);
	indexesItem->setText(0, trLabel(trSysIndexes).arg(sysIx.size()));
	indexesItem->setIcon(0, Utils::getIcon("index.png"));
	indexesItem->setText(1, schema);
	for (int i = 0; i < sysIx.size(); ++i)
	{
		QTreeWidgetItem *indexItem = new QTreeWidgetItem(indexesItem, SysIndexType);
		indexItem->setText(0, sysIx.at(i));
		indexItem->setText(1, schema);
	}
}

void TableTree::buildTriggers(QTreeWidgetItem *triggersItem, const QString & schema, const QString & table)
{
	deleteChildren(triggersItem);
	QStringList values = Database::getObjects("trigger", schema).values(table.toLower());
	triggersItem->setText(0, trLabel(trTriggers).arg(values.size()));
	triggersItem->setIcon(0, Utils::getIcon("trigger.png"));
	triggersItem->setText(1, schema);
	for (int i = 0; i < values.size(); ++i)
	{
		QTreeWidgetItem *triggerItem = new QTreeWidgetItem(triggersItem, TriggerType);
		triggerItem->setText(0, values.at(i));
		triggerItem->setText(1, schema);
	}
}

void TableTree::buildViews(QTreeWidgetItem * viewsItem, const QString & schema)
{
	deleteChildren(viewsItem);

	// Build views tree
	QStringList views = Database::getObjects("view", schema).keys();
	viewsItem->setText(0, trLabel(trViews).arg(views.size()));
	viewsItem->setText(1, schema);
	foreach(QString view, views)
	{
		QTreeWidgetItem * viewItem = new QTreeWidgetItem(viewsItem, ViewType);
		viewItem->setText(0, view);
		viewItem->setText(1, schema);
		QTreeWidgetItem *triggersItem = new QTreeWidgetItem(viewItem, TriggersItemType);
		buildTriggers(triggersItem, schema, view);
	}
}

void TableTree::buildCatalogue(QTreeWidgetItem * systemItem, const QString & schema)
{
	deleteChildren(systemItem);

	QStringList values = Database::getSysObjects(schema).keys();
	systemItem->setText(0, trLabel(trSys).arg(values.size()));
	systemItem->setText(1, schema);
	foreach(QString i, values)
	{
		QTreeWidgetItem * sysItem = new QTreeWidgetItem(systemItem, SystemType);
		sysItem->setText(0, i);
		sysItem->setText(1, schema);
	}
}


void TableTree::deleteChildren(QTreeWidgetItem * item)
{
	QList<QTreeWidgetItem *> items = item->takeChildren();
	foreach (QTreeWidgetItem *item, items)
	{
		delete item;
	}
}

QString TableTree::trLabel(const QString & trStr)
{
	return trStr + " (%1)";
}

QList<QTreeWidgetItem*> TableTree::searchMask(const QString & trStr)
{
	return findItems(trStr + " (", Qt::MatchStartsWith | Qt::MatchRecursive, 0);
}

void TableTree::buildViewTree(QString schema, QString name)
{
	foreach (QTreeWidgetItem* item, searchMask(trViews))
	{
		if (item->text(1) == schema && item->type() == ViewsItemType)
			buildViews(item, schema);
	}
}

void TableTree::mousePressEvent(QMouseEvent *event)
{
	if (event->button() == Qt::LeftButton)
		m_dragStartPosition = event->pos();
	QTreeWidget::mousePressEvent(event);
}

 void TableTree::mouseMoveEvent(QMouseEvent *event)
{
	if (!(event->buttons() & Qt::LeftButton))
		return;
	if ((event->pos() - m_dragStartPosition).manhattanLength()
			< QApplication::startDragDistance())
		return;

	if (currentItem()->type() != TableTree::TableType &&
		   currentItem()->type() != TableTree::ViewType &&
		   currentItem()->type() != TableTree::DatabaseItemType &&
		   currentItem()->type() != TableTree::IndexType &&
		   currentItem()->type() != TableTree::TriggerType &&
		   currentItem()->type() != TableTree::SystemType &&
		   currentItem()->type() != TableTree::ColumnType)
		return;

#if QT_VERSION >= 0x040300
	QDrag *drag = new QDrag(this);
	QMimeData *mimeData = new QMimeData;

	mimeData->setText(currentItem()->text(0));
	drag->setMimeData(mimeData);
	drag->exec(Qt::CopyAction);
#endif

	QTreeWidget::mouseMoveEvent(event);
}
