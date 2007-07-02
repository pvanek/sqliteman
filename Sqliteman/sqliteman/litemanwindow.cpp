/*
For general Sqliteman copyright and licensing information please refer
to the COPYING file provided with the program. Following this notice may exist
a copyright and/or license notice that predates the release of Sqliteman
for which a new license (GPL+exception) is in place.
*/
#include <QTreeWidget>
#include <QTableView>
#include <QSplitter>
#include <QMenuBar>
#include <QMenu>
#include <QProcess>
#include <QTime>

#include <QInputDialog>
#include <QMessageBox>
#include <QFileDialog>

#include <QSqlDatabase>
#include <QSqlError>

#include <QCoreApplication>
#include <QCloseEvent>
#include <QSettings>
#include <QFileInfo>
#include <QAction>
#include <QFile>
#include <QDir>

#include "litemanwindow.h"
#include "preferencesdialog.h"
#include "queryeditordialog.h"
#include "createtabledialog.h"
#include "createtriggerdialog.h"
#include "createviewdialog.h"
#include "alterviewdialog.h"
#include "altertabledialog.h"
#include "altertriggerdialog.h"
#include "dataviewer.h"
#include "tabletree.h"
#include "database.h"
#include "sqleditor.h"
#include "sqlmodels.h"
#include "createindexdialog.h"
#include "constraintsdialog.h"
#include "analyzedialog.h"
#include "vacuumdialog.h"
#include "helpbrowser.h"
#include "sqlparser.h"


LiteManWindow::LiteManWindow(const QString & fileToOpen)
	: QMainWindow(),
	m_mainDbPath(""),
	m_appName("Sqliteman"),
	helpBrowser(0)
{
	recentDocs.clear();
	attachedDb.clear();
	initUI();
	initActions();
	initMenus();
// 	initStatusBar();
	statusBar();
	readSettings();

	// Check command line
	if (!fileToOpen.isNull())
		open(fileToOpen);
}

void LiteManWindow::closeEvent(QCloseEvent * e)
{
	sqlEditor->saveOnExit();

	QMapIterator<QString, QString> i(attachedDb);
	while (i.hasNext())
	{
		i.next();
		QSqlDatabase::database(i.value()).rollback();
		QSqlDatabase::database(i.value()).close();
	}

	writeSettings();
	e->accept();
}


void LiteManWindow::initUI()
{
	setWindowTitle(m_appName);
	splitter = new QSplitter(this);
	splitterSql = new QSplitter(Qt::Vertical, this);

	tableTree = new TableTree();
	dataViewer = new DataViewer();
	sqlEditor = new SqlEditor(this);

	splitterSql->addWidget(sqlEditor);
	splitterSql->addWidget(dataViewer);
	splitterSql->setCollapsible(0, false);
	splitterSql->setCollapsible(1, false);

	splitter->addWidget(tableTree);
	splitter->addWidget(splitterSql);
	splitter->setCollapsible(0, false);
	splitter->setCollapsible(1, false);

	setCentralWidget(splitter);

	// Disable the UI, as long as there is no open database
	tableTree->setEnabled(false);
	dataViewer->setEnabled(false);
	sqlEditor->setEnabled(false);
	sqlEditor->hide();

	connect(tableTree, SIGNAL(itemActivated(QTreeWidgetItem *, int)),
		this, SLOT(treeItemActivated(QTreeWidgetItem *, int)));

	connect(tableTree, SIGNAL(customContextMenuRequested(const QPoint &)),
		this, SLOT(treeContextMenuOpened(const QPoint &)));
	connect(tableTree, SIGNAL(currentItemChanged(QTreeWidgetItem*,QTreeWidgetItem*)),
			this, SLOT(tableTree_currentItemChanged(QTreeWidgetItem*,QTreeWidgetItem*)));

	// sql editor
	connect(sqlEditor, SIGNAL(showSqlResult(QString)), this, SLOT(execSql(QString)));
	connect(sqlEditor, SIGNAL(rebuildViewTree(QString, QString)),
			tableTree, SLOT(buildViewTree(QString,QString)));
}

void LiteManWindow::initActions()
{
	newAct = new QAction(tr("&New..."), this);
	newAct->setShortcut(tr("Ctrl+N"));
	connect(newAct, SIGNAL(triggered()), this, SLOT(newDB()));

	openAct = new QAction(tr("&Open..."), this);
	openAct->setShortcut(tr("Ctrl+O"));
	connect(openAct, SIGNAL(triggered()), this, SLOT(open()));

	recentAct = new QAction(tr("&Recent Databases"), this);
	recentFilesMenu = new QMenu(this);
	recentAct->setMenu(recentFilesMenu);

	preferencesAct = new QAction(tr("&Preferences..."), this);
	connect(preferencesAct, SIGNAL(triggered()), this, SLOT(preferences()));

	exitAct = new QAction(tr("E&xit"), this);
	exitAct->setShortcut(tr("Ctrl+Q"));
	connect(exitAct, SIGNAL(triggered()), this, SLOT(close()));

	aboutAct = new QAction(tr("&About..."), this);
	connect(aboutAct, SIGNAL(triggered()), this, SLOT(about()));

	aboutQtAct = new QAction(tr("About &Qt..."), this);
	connect(aboutQtAct, SIGNAL(triggered()), this, SLOT(aboutQt()));

	helpAct = new QAction(tr("&Help Content..."), this);
	helpAct->setShortcut(tr("F1"));
	connect(helpAct, SIGNAL(triggered()), this, SLOT(help()));

	execSqlAct = new QAction(tr("&SQL Editor"), this);
	execSqlAct->setShortcut(tr("Ctrl+E"));
	execSqlAct->setCheckable(true);
	connect(execSqlAct, SIGNAL(triggered()), this, SLOT(handleSqlEditor()));

	objectBrowserAct = new QAction(tr("Object &Browser"), this);
	objectBrowserAct->setShortcut(tr("Ctrl+B"));
	objectBrowserAct->setCheckable(true);
	connect(objectBrowserAct, SIGNAL(triggered()), this, SLOT(handleObjectBrowser()));

	buildQueryAct = new QAction(tr("&Build Query..."), this);
	buildQueryAct->setShortcut(tr("Ctrl+R"));
	connect(buildQueryAct, SIGNAL(triggered()), this, SLOT(buildQuery()));

	exportSchemaAct = new QAction(tr("&Export Schema..."), this);
	connect(exportSchemaAct, SIGNAL(triggered()), this, SLOT(exportSchema()));

	dumpDatabaseAct = new QAction(tr("&Dump Database..."), this);
	connect(dumpDatabaseAct, SIGNAL(triggered()), this, SLOT(dumpDatabase()));

	createTableAct = new QAction(tr("&Create Table..."), this);
	createTableAct->setShortcut(tr("Ctrl+T"));
	connect(createTableAct, SIGNAL(triggered()), this, SLOT(createTable()));

	dropTableAct = new QAction(tr("&Drop Table"), this);
	connect(dropTableAct, SIGNAL(triggered()), this, SLOT(dropTable()));

	alterTableAct = new QAction(tr("&Alter Table..."), this);
	alterTableAct->setShortcut(tr("Ctrl+A"));
	connect(alterTableAct, SIGNAL(triggered()), this, SLOT(alterTable()));

	createViewAct = new QAction(tr("Create &View..."), this);
	createViewAct->setShortcut(tr("Ctrl+G"));
	connect(createViewAct, SIGNAL(triggered()), this, SLOT(createView()));

	dropViewAct = new QAction(tr("&Drop View"), this);
	connect(dropViewAct, SIGNAL(triggered()), this, SLOT(dropView()));

	alterViewAct = new QAction(tr("&Alter View..."), this);
	connect(alterViewAct, SIGNAL(triggered()), this, SLOT(alterView()));

	createIndexAct = new QAction(tr("&Create Index..."), this);
	connect(createIndexAct, SIGNAL(triggered()), this, SLOT(createIndex()));

	dropIndexAct = new QAction(tr("&Drop Index"), this);
	connect(dropIndexAct, SIGNAL(triggered()), this, SLOT(dropIndex()));

	describeTableAct = new QAction(tr("D&escribe Table"), this);
	connect(describeTableAct, SIGNAL(triggered()), this, SLOT(describeTable()));

	createTriggerAct = new QAction(tr("&Create Trigger..."), this);
	connect(createTriggerAct, SIGNAL(triggered()), this, SLOT(createTrigger()));

	alterTriggerAct = new QAction(tr("&Alter Trigger..."), this);
	connect(alterTriggerAct, SIGNAL(triggered()), this, SLOT(alterTrigger()));

	dropTriggerAct = new QAction(tr("&Drop Trigger"), this);
	connect(dropTriggerAct, SIGNAL(triggered()), this, SLOT(dropTrigger()));

	describeTriggerAct = new QAction(tr("D&escribe Trigger"), this);
	connect(describeTriggerAct, SIGNAL(triggered()), this, SLOT(describeTrigger()));

	describeViewAct = new QAction(tr("D&escribe View"), this);
	connect(describeViewAct, SIGNAL(triggered()), this, SLOT(describeView()));

	describeIndexAct = new QAction(tr("D&escribe Index"), this);
	connect(describeIndexAct, SIGNAL(triggered()), this, SLOT(describeIndex()));

	reindexAct = new QAction(tr("&Reindex"), this);
	connect(reindexAct, SIGNAL(triggered()), this, SLOT(reindex()));

	analyzeAct = new QAction(tr("&Analyze Statistics..."), this);
	connect(analyzeAct, SIGNAL(triggered()), this, SLOT(analyzeDialog()));

	vacuumAct = new QAction(tr("&Vacuum..."), this);
	connect(vacuumAct, SIGNAL(triggered()), this, SLOT(vacuumDialog()));

	attachAct = new QAction(tr("A&ttach Database..."), this);
	connect(attachAct, SIGNAL(triggered()), this, SLOT(attachDatabase()));

	detachAct = new QAction(tr("&Detach Database"), this);
	connect(detachAct, SIGNAL(triggered()), this, SLOT(detachDatabase()));

	refreshTreeAct = new QAction(tr("&Refresh Object Tree"), this);
	connect(refreshTreeAct, SIGNAL(triggered()), tableTree, SLOT(buildTree()));

	consTriggAct = new QAction(tr("&Constraint Triggers..."), this);
	connect(consTriggAct, SIGNAL(triggered()), this, SLOT(constraintTriggers()));
}

void LiteManWindow::initMenus()
{
	QMenu * fileMenu = menuBar()->addMenu(tr("&File"));
	fileMenu->addAction(newAct);
	fileMenu->addAction(openAct);
	fileMenu->addAction(recentAct);
	fileMenu->addSeparator();
	fileMenu->addAction(preferencesAct);
	fileMenu->addSeparator();
	fileMenu->addAction(exitAct);

	contextMenu = menuBar()->addMenu(tr("&Context"));
	contextMenu->setEnabled(false);

	databaseMenu = menuBar()->addMenu(tr("&Database"));
	databaseMenu->addAction(createTableAct);
	databaseMenu->addAction(createViewAct);
	databaseMenu->addSeparator();
	databaseMenu->addAction(buildQueryAct);
	databaseMenu->addAction(execSqlAct);
	databaseMenu->addAction(objectBrowserAct);
	databaseMenu->addSeparator();
	databaseMenu->addAction(exportSchemaAct);
	databaseMenu->addAction(dumpDatabaseAct);

	adminMenu = menuBar()->addMenu(tr("&System"));
	adminMenu->addAction(analyzeAct);
	adminMenu->addAction(vacuumAct);
	adminMenu->addSeparator();
	adminMenu->addAction(attachAct);

	QMenu * helpMenu = menuBar()->addMenu(tr("&Help"));
	helpMenu->addAction(helpAct);
	helpMenu->addAction(aboutAct);
	helpMenu->addAction(aboutQtAct);

	databaseMenu->setEnabled(false);
	adminMenu->setEnabled(false);
}

void LiteManWindow::updateRecent(QString fn)
{
	if (recentDocs.indexOf(fn) == -1)
		recentDocs.prepend(fn);
	else
		recentDocs.replace(recentDocs.indexOf(fn), fn);
	rebuildRecentFileMenu();
}

void LiteManWindow::removeRecent(QString fn)
{
	if (recentDocs.indexOf(fn) != -1)
		recentDocs.removeAt(recentDocs.indexOf(fn));
	rebuildRecentFileMenu();
}

void LiteManWindow::rebuildRecentFileMenu()
{
	recentFilesMenu->clear();
	uint max = qMin(PreferencesDialog::recentlyUsedCount(), recentDocs.count());
	QFile fi;

	for (uint i = 0; i < max; ++i)
	{
		fi.setFileName(recentDocs.at(i));
		if (!fi.exists())
		{
			removeRecent(recentDocs.at(i));
			break;
		}
		QAction *a = new QAction(QString("&%1 %2").arg(i+1).arg(recentDocs.at(i)), this);
		a->setData(QVariant(recentDocs.at(i)));
		connect(a, SIGNAL(triggered()), this, SLOT(openRecent()));
		recentFilesMenu->addAction(a);
	}
	recentAct->setDisabled(recentDocs.count() == 0);
}

void LiteManWindow::readSettings()
{
	QSettings settings("yarpen.cz", "sqliteman");

	restoreGeometry(settings.value("window/geometry").toByteArray());
	QByteArray splitterData = settings.value("window/splitter").toByteArray();

	splitter->restoreState(splitterData);
	splitterSql->restoreState(settings.value("sqleditor/splitter").toByteArray());
	dataViewer->restoreSplitter(settings.value("dataviewer/splitter").toByteArray());

	sqlEditor->setVisible(settings.value("sqleditor/show", true).toBool());
	tableTree->setVisible(settings.value("objectbrowser/show", true).toBool());
	objectBrowserAct->setChecked(settings.value("objectbrowser/show", false).toBool());
	execSqlAct->setChecked(settings.value("sqleditor/show", false).toBool());

	QString fn(settings.value("sqleditor/filename", QString()).toString());
	if (!fn.isNull() && !fn.isEmpty())
	   sqlEditor->setFileName(fn);

	recentDocs = settings.value("recentDocs/files").toStringList();
	rebuildRecentFileMenu();
}

void LiteManWindow::writeSettings()
{
	QSettings settings("yarpen.cz", "sqliteman");

	settings.setValue("window/geometry", saveGeometry());
	settings.setValue("window/size", size());
	settings.setValue("window/splitter", splitter->saveState());
	settings.setValue("objectbrowser/show", tableTree->isVisible());
	settings.setValue("sqleditor/show", sqlEditor->isVisible());
	settings.setValue("sqleditor/splitter", splitterSql->saveState());
	settings.setValue("sqleditor/filename", sqlEditor->fileName());
	settings.setValue("dataviewer/splitter", dataViewer->saveSplitter());
	settings.setValue("recentDocs/files", recentDocs);
}

void LiteManWindow::newDB()
{
	// Creating a database is virtually the same as opening an existing one. SQLite simply creates
	// a database which doesn't already exist
	QString fileName = QFileDialog::getSaveFileName(this, tr("New Database"), QDir::currentPath(), tr("SQLite database (*)"));

	if(fileName.isNull())
		return;

	if (QFile::exists(fileName))
		QFile::remove(fileName);

	openDatabase(fileName);
}

void LiteManWindow::open(const QString & file)
{
	QString fileName;

	// If no file name was provided, open dialog
	if(!file.isNull())
		fileName = file;
	else
		fileName = QFileDialog::getOpenFileName(this,
			                    tr("Open Database"),
			                    QDir::currentPath(),
			                    tr("SQLite database (*)"));

	if(fileName.isNull())
		return;
	openDatabase(fileName);
}

void LiteManWindow::openDatabase(const QString & fileName)
{
	QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE", SESSION_NAME);
	db.setDatabaseName(fileName);

	if(!db.open())
	{
		QString msg = tr("Unable to open or create file %1. It is probably not a database").arg(QFileInfo(fileName).fileName());
		QMessageBox::warning(this, m_appName, msg);
		return;
	}

	attachedDb.clear();
	attachedDb["main"] = SESSION_NAME;

	QFileInfo fi(fileName);
	QDir::setCurrent(fi.absolutePath());
	m_mainDbPath = fi.absoluteFilePath();
	updateRecent(fileName);

	// Build tree & clean model
	tableTree->buildTree();
	dataViewer->setTableModel(new QSqlQueryModel());

	// Update the title
	setWindowTitle(QString("%1 - %2").arg(fi.fileName()).arg(m_appName));

	// Enable UI (Only the tree, the data viewer will be enabled upon demand)
	tableTree->setEnabled(true);
	databaseMenu->setEnabled(true);
	adminMenu->setEnabled(true);
	sqlEditor->setEnabled(true);
}

void LiteManWindow::openRecent()
{
	QAction *action = qobject_cast<QAction *>(sender());
	if (action)
		open(action->data().toString());
}

void LiteManWindow::about()
{
	QMessageBox::about(this, tr("About"),
					   tr("Sqliteman - SQLite databases made easy\n\n"
						  "Version %1\n"
						  "(c) 2007 Petr Vanek").arg(SQLITEMAN_VERSION));
}

void LiteManWindow::aboutQt()
{
	QMessageBox::aboutQt(this, m_appName);
}

void LiteManWindow::help()
{
	if (!helpBrowser)
		helpBrowser = new HelpBrowser(m_lang, this);
	helpBrowser->show();
}

void LiteManWindow::buildQuery()
{
	QueryEditorDialog dlg(QueryEditorDialog::BuildQuery, this);

	dlg.resize(500, 400);
	int ret = dlg.exec();

	if(ret == QDialog::Accepted)
		runQuery(dlg.statement());
}

void LiteManWindow::handleSqlEditor()
{
	sqlEditor->setVisible(!sqlEditor->isVisible());
	execSqlAct->setChecked(sqlEditor->isVisible());
}
#include <QtDebug>
void LiteManWindow::handleObjectBrowser()
{
	qDebug() << "1";
	tableTree->setVisible(!tableTree->isVisible());
	objectBrowserAct->setChecked(tableTree->isVisible());
}

void LiteManWindow::execSql(QString query)
{
	if (query.isEmpty() || query.isNull())
	{
		QMessageBox::warning(this, tr("No SQL statement"), tr("You are trying to run an undefined SQL query. Hint: select your query in the editor"));
		return;
	}

	sqlEditor->setStatusMessage();
	QTime time;
	time.start();

	// Run query
	SqlQueryModel * model = new SqlQueryModel(this);
	model->setQuery(query, QSqlDatabase::database(SESSION_NAME));

	if (!dataViewer->setTableModel(model))
		return;

	dataViewer->setEnabled(true);
	dataViewer->showButtons(false);
	dataViewer->showStatusText(true);

	sqlEditor->setStatusMessage(tr("Duration: %1 seconds").arg(time.elapsed() / 1000.0));
	
	// Check For Error in the SQL
	if(model->lastError().isValid())
		dataViewer->setStatusText(tr("Query Error: %1\n\n%2").arg(model->lastError().databaseText()).arg(query));
	else
	{
		dataViewer->setStatusText(tr("Query OK\nRow(s) returned: %1\n\n%2").arg(model->rowCount()).arg(query));
		if (SqlEditorTools::SqlParser::updateTree(query))
			tableTree->buildTree();
	}
}

void LiteManWindow::exportSchema()
{
	QString fileName = QFileDialog::getSaveFileName(this, tr("Export Schema"), QDir::currentPath(), tr("SQL File (*.sql)"));

	if (fileName.isNull())
		return;

	Database::exportSql(fileName);
}

void LiteManWindow::dumpDatabase()
{
	QString fileName = QFileDialog::getSaveFileName(this, tr("Export Database"), QDir::currentPath(), tr("SQL File (*.sql)"));

	if (fileName.isNull())
		return;

	QProcess dump;
	dump.setStandardOutputFile(fileName);
	dump.start(SQLITE_BINARY, QStringList() << m_mainDbPath << ".dump");
	if (!dump.waitForStarted() || !dump.waitForFinished(-1) || (dump.exitStatus() != QProcess::NormalExit))
	{
		QString reason;
		switch (dump.error())
		{
			case (QProcess::FailedToStart) :
				reason = tr("The process failed to start. Either the invoked program is missing, or you may have insufficient permissions to invoke the program.");
				break;
			case (QProcess::Crashed) :
				reason = tr("The process crashed some time after starting successfully.");
				break;
			case (QProcess::WriteError) :
				reason = tr("An error occurred when attempting to write to the process. For example, the process may not be running, or it may have closed its input channel.");
				break;
			case (QProcess::ReadError):
				reason = tr("An error occurred when attempting to read from the process. For example, the process may not be running.");
				break;
			default:
				reason = tr("An unknown error occurred.");
		}
		QMessageBox::warning(this, m_appName, tr("Error creating the dump. Reason: %1").arg(reason));
	}
	else
		QMessageBox::warning(this, m_appName, tr("Dump written into: %1").arg(fileName));
}

void LiteManWindow::createTable()
{
	CreateTableDialog dlg(this);
	dlg.exec();
	if (dlg.update)
	{
		foreach (QTreeWidgetItem* item, tableTree->searchMask(tableTree->trTables))
		{
			if (item->type() == TableTree::TablesItemType)
				tableTree->buildTables(item, item->text(1));
		}
	}
}

void LiteManWindow::alterTable()
{
	QTreeWidgetItem * item = tableTree->currentItem();

	if(!item)
		return;

	AlterTableDialog dlg(this, item->text(0), item->text(1));
	dlg.exec();
	if (dlg.update)
		tableTree->buildTables(item->parent(), item->text(1));
}

void LiteManWindow::dropTable()
{
	QTreeWidgetItem * item = tableTree->currentItem();

	if(!item)
		return;

	int ret = QMessageBox::question(this, m_appName,
					tr("Are you sure that you wish to drop the table \"%1\"?").arg(item->text(0)),
					QMessageBox::Yes, QMessageBox::No);

	if(ret == QMessageBox::Yes)
	{
		if (Database::dropTable(item->text(0), item->text(1)))
			tableTree->buildTables(item->parent(), item->text(1));
	}
}

void LiteManWindow::createView()
{
	CreateViewDialog dia("", "", this);

	dia.exec();
	if(dia.update)
		tableTree->buildViewTree(dia.schema(), dia.name());
}

void LiteManWindow::alterView()
{
	QTreeWidgetItem * item = tableTree->currentItem();
	AlterViewDialog dia(item->text(0), item->text(1), this);
	dia.exec();
	if (dia.update)
		tableTree->buildViewTree(item->text(1), item->text(0));
}

void LiteManWindow::dropView()
{
	QTreeWidgetItem * item = tableTree->currentItem();

	if(!item)
		return;

	// Ask the user for confirmation
	int ret = QMessageBox::question(this, m_appName,
					tr("Are you sure that you wish to drop the view \"%1\"?").arg(item->text(0)),
					QMessageBox::Yes, QMessageBox::No);

	if(ret == QMessageBox::Yes)
	{
		if (Database::dropView(item->text(0), item->text(1)))
			tableTree->buildViews(item->parent(), item->text(1));
	}
}

void LiteManWindow::createIndex()
{
	QString table(tableTree->currentItem()->parent()->text(0));
	QString schema(tableTree->currentItem()->parent()->text(1));
	CreateIndexDialog dia(table, schema, this);
	dia.exec();
	if (dia.update)
		tableTree->buildIndexes(tableTree->currentItem(), schema, table);
}

void LiteManWindow::dropIndex()
{
	QTreeWidgetItem * item = tableTree->currentItem();

	if(!item)
		return;

	// Ask the user for confirmation
	int ret = QMessageBox::question(this, m_appName,
					tr("Are you sure that you wish to drop the index \"%1\"?").arg(item->text(0)),
					QMessageBox::Yes, QMessageBox::No);

	if(ret == QMessageBox::Yes)
	{
		if (Database::dropIndex(item->text(0), item->text(1)))
			tableTree->buildIndexes(item->parent(), item->text(1), item->text(0));
	}
}

void LiteManWindow::treeItemActivated(QTreeWidgetItem * item, int /*column*/)
{
	if(!item)
		return;

	if (item->type() == TableTree::TableType || item->type() == TableTree::ViewType
	    || item->type() == TableTree::SystemType)
	{
// 		sqlEditor->setVisible(false);
		dataViewer->setEnabled(true);
// 		dataViewer->showStatusText(false);

		if(item->type() == TableTree::ViewType || item->type() == TableTree::SystemType)
		{
			dataViewer->showButtons(false);
			SqlQueryModel * model = new SqlQueryModel(this);
			model->setQuery(QString("select * from %1.%2").arg(item->text(1)).arg(item->text(0)), QSqlDatabase::database(SESSION_NAME));
			dataViewer->setTableModel(model);
		}
		else
		{
			dataViewer->showButtons(true);
			SqlTableModel * model = new SqlTableModel(this, QSqlDatabase::database(attachedDb[item->text(1)]));
			model->setSchema(item->text(1));
			model->setTable(item->text(0));
			model->select();
			model->setEditStrategy(SqlTableModel::OnManualSubmit);
			dataViewer->setTableModel(model);
		}
	}
}

void LiteManWindow::tableTree_currentItemChanged(QTreeWidgetItem* cur, QTreeWidgetItem* prev)
{
	buildContextMenu(cur);
}

void LiteManWindow::buildContextMenu(QTreeWidgetItem * item)
{
	contextMenu->clear();
	if (!item)
		return;

	switch(item->type())
	{
		case TableTree::TablesItemType:
			contextMenu->addAction(createTableAct);
			break;

		case TableTree::ViewsItemType:
			contextMenu->addAction(createViewAct);
			break;

		case TableTree::TableType:
			contextMenu->addAction(describeTableAct);
			contextMenu->addAction(alterTableAct);
			contextMenu->addAction(dropTableAct);
			contextMenu->addAction(reindexAct);
			break;

		case TableTree::ViewType:
			contextMenu->addAction(describeViewAct);
			contextMenu->addAction(alterViewAct);
			contextMenu->addAction(dropViewAct);
			break;

		case TableTree::IndexType:
			contextMenu->addAction(describeIndexAct);
			contextMenu->addAction(dropIndexAct);
			contextMenu->addAction(reindexAct);
			break;

		case TableTree::IndexesItemType:
			contextMenu->addAction(createIndexAct);
			break;

		case TableTree::DatabaseItemType:
			contextMenu->addAction(refreshTreeAct);
			if (item->text(0) != "main")
				contextMenu->addAction(detachAct);
			break;

		case TableTree::TriggersItemType:
			contextMenu->addAction(createTriggerAct);
			contextMenu->addAction(consTriggAct);
			break;

		case TableTree::TriggerType:
			contextMenu->addAction(describeTriggerAct);
			contextMenu->addAction(alterTriggerAct);
			contextMenu->addAction(dropTriggerAct);
			break;
	}
	contextMenu->setDisabled(contextMenu->actions().count() == 0);
}


void LiteManWindow::treeContextMenuOpened(const QPoint & pos)
{
	if (contextMenu->actions().count() != 0)
		contextMenu->exec(tableTree->viewport()->mapToGlobal(pos));
}

void LiteManWindow::describeTable()
{
	runQuery(QString("pragma \"%1\".table_info (\"%2\");")
			.arg(tableTree->currentItem()->text(1))
			.arg(tableTree->currentItem()->text(0)));
}

void LiteManWindow::describeView()
{
	runQuery(QString("select sql from \"%1\".sqlite_master where name = '%2';")
			.arg(tableTree->currentItem()->text(1))
			.arg(tableTree->currentItem()->text(0)));
}

void LiteManWindow::describeIndex()
{
	runQuery(QString("pragma \"%1\".index_info (\"%2\");")
			.arg(tableTree->currentItem()->text(1))
			.arg(tableTree->currentItem()->text(0)));
}

void LiteManWindow::describeTrigger()
{
	runQuery(QString("select sql from \"%1\".sqlite_master where name = '%2';")
			.arg(tableTree->currentItem()->text(1))
			.arg(tableTree->currentItem()->text(0)));
}

void LiteManWindow::runQuery(QString statement)
{
	SqlQueryModel * model = new SqlQueryModel(this);
	model->setQuery(statement, QSqlDatabase::database(SESSION_NAME));
	if (!dataViewer->setTableModel(model))
		return;

	dataViewer->setEnabled(true);
	dataViewer->showStatusText(true);
	dataViewer->showButtons(false);

	if(model->lastError().isValid())
		dataViewer->setStatusText(tr("Query Error: %1").arg(model->lastError().databaseText()));
	else
		dataViewer->setStatusText(tr("Query OK\nRow(s) returned: %1").arg(model->rowCount()));
}

void LiteManWindow::analyzeDialog()
{
	AnalyzeDialog *dia = new AnalyzeDialog(this);
	dia->exec();
	delete dia;
	foreach (QTreeWidgetItem* item, tableTree->searchMask(tableTree->trSys))
	{
		if (item->type() == TableTree::SystemItemType)
			tableTree->buildCatalogue(item, item->text(1));
	}
}

void LiteManWindow::vacuumDialog()
{
	VacuumDialog *dia = new VacuumDialog(this);
	dia->exec();
	delete dia;
}

void LiteManWindow::attachDatabase()
{
	QString fileName;
	fileName = QFileDialog::getOpenFileName(this, tr("Attach Database"), QDir::currentPath(), tr("SQLite database (*)"));
	if (fileName.isEmpty())
		return;
	bool ok;
	QFileInfo f(fileName);
	QString schema = QInputDialog::getText(this, tr("Attach Database"),
                                          tr("Enter a Schema Alias:"), QLineEdit::Normal,
                                          f.baseName(), &ok);
	if (!ok || schema.isEmpty())
		return;
	if (!Database::execSql(QString("attach database '%1' as \"%2\";").arg(fileName).arg(schema)))
		return;

	attachedDb[schema] = Database::sessionName(schema);
	QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE", attachedDb[schema]);
	db.setDatabaseName(fileName);
	if(!db.open())
	{
		QString msg = tr("Unable to open or create file %1. It is probably not a database").arg(QFileInfo(fileName).fileName());
		QMessageBox::warning(this, "", msg);
		return;
	}

	tableTree->buildDatabase(schema);
}

void LiteManWindow::detachDatabase()
{
	QString dbname(tableTree->currentItem()->text(0));
	if (!Database::execSql(QString("detach database \"%1\";").arg(dbname)))
		return;

	QSqlDatabase::database(attachedDb[dbname]).rollback();
	QSqlDatabase::database(attachedDb[dbname]).close();
	attachedDb.remove(dbname);

	delete tableTree->currentItem();
}

void LiteManWindow::createTrigger()
{
	QString table(tableTree->currentItem()->parent()->text(0));
	QString schema(tableTree->currentItem()->parent()->text(1));
	CreateTriggerDialog *dia = new CreateTriggerDialog(table, schema, this);
	dia->exec();
	if (dia->update)
		tableTree->buildTriggers(tableTree->currentItem(), schema, table);
	delete dia;
}

void LiteManWindow::alterTrigger()
{
	QString table(tableTree->currentItem()->text(0));
	QString schema(tableTree->currentItem()->text(1));
	AlterTriggerDialog *dia = new AlterTriggerDialog(table, schema, this);
	dia->exec();
	delete dia;
}


void LiteManWindow::dropTrigger()
{
	QTreeWidgetItem * item = tableTree->currentItem();

	if(!item)
		return;

	// Ask the user for confirmation
	int ret = QMessageBox::question(this, m_appName,
					tr("Are you sure that you wish to drop the trigger \"%1\"?").arg(item->text(0)),
					QMessageBox::Yes, QMessageBox::No);

	if(ret == QMessageBox::Yes)
	{
		if (Database::dropTrigger(item->text(0), item->text(1)))
			tableTree->buildTriggers(item->parent(), item->text(1), item->parent()->parent()->text(0));
	}
}

void LiteManWindow::constraintTriggers()
{
	QString table(tableTree->currentItem()->parent()->text(0));
	QString schema(tableTree->currentItem()->parent()->text(1));
	ConstraintsDialog dia(table, schema, this);
	dia.exec();
	if (dia.update)
		tableTree->buildTriggers(tableTree->currentItem(), schema, table);
}

void LiteManWindow::reindex()
{
	QTreeWidgetItem * item = tableTree->currentItem();
	if(!item)
		return;
	QString sql(QString("REINDEX \"%1\".\"%2\";").arg(item->text(1)).arg(item->text(0)));
	Database::execSql(sql);
}

void LiteManWindow::preferences()
{
	PreferencesDialog prefs(this);
	if (prefs.exec())
		prefs.saveSettings();
}
