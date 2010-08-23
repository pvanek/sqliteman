/*
For general Sqliteman copyright and licensing information please refer
to the COPYING file provided with the program. Following this notice may exist
a copyright and/or license notice that predates the release of Sqliteman
for which a new license (GPL+exception) is in place.
*/

#include <QMessageBox>
#include <QSqlQuery>
#include <QSqlError>
#include <QKeyEvent>
#include <QClipboard>
#include <QDateTime>
#include <QHeaderView>
#include <QResizeEvent>
#include <QSettings>
#include <QInputDialog>

#include "dataviewer.h"
#include "dataexportdialog.h"
#include "sqlmodels.h"
#include "database.h"
#include "sqldelegate.h"
#include "utils.h"
#include "blobpreviewwidget.h"


DataViewer::DataViewer(QWidget * parent)
	: QMainWindow(parent),
	  dataResized(true)
{
	ui.setupUi(this);

#ifdef Q_WS_MAC
    ui.mainToolBar->setIconSize(QSize(16, 16));
    ui.exportToolBar->setIconSize(QSize(16, 16));
    ui.snapshotToolBar->setIconSize(QSize(16, 16));
#endif

	handleBlobPreview(false);

	ui.splitter->setCollapsible(0, false);
	ui.splitter->setCollapsible(1, false);
	ui.actionNew_Row->setIcon(Utils::getIcon("insert_table_row.png"));
	ui.actionRemove_Row->setIcon(Utils::getIcon("delete_table_row.png"));
	ui.actionTruncate_Table->setIcon(Utils::getIcon("clear_table_contents.png"));
	ui.actionCommit->setIcon(Utils::getIcon("database_commit.png"));
	ui.actionRollback->setIcon(Utils::getIcon("database_rollback.png"));
	ui.actionRipOut->setIcon(Utils::getIcon("snapshot.png"));
	ui.actionBLOB_Preview->setIcon(Utils::getIcon("blob.png"));
	ui.actionExport_Data->setIcon(Utils::getIcon("document-export.png"));
	ui.actionClose->setIcon(Utils::getIcon("close.png"));
	ui.action_Goto_Line->setIcon(Utils::getIcon("go-next-use.png"));

    actInsertNull = new QAction(Utils::getIcon("setnull.png"), tr("Insert NULL"), ui.tableView);
    connect(actInsertNull, SIGNAL(triggered()), this, SLOT(actInsertNull_triggered()));
    actOpenEditor = new QAction(Utils::getIcon("edit.png"), tr("Open Data Editor..."), ui.tableView);
    connect(actOpenEditor, SIGNAL(triggered()), this, SLOT(actOpenEditor_triggered()));
    ui.tableView->addAction(actInsertNull);
    ui.tableView->addAction(actOpenEditor);

	// custom delegate
	ui.tableView->setItemDelegate(new SqlDelegate(this));

	// workaround for Ctrl+C
	DataViewerTools::KeyPressEater *keyPressEater = new DataViewerTools::KeyPressEater(this);
	ui.tableView->installEventFilter(keyPressEater);

	QSettings settings("yarpen.cz", "sqliteman");
	restoreState(settings.value("dataviewer/state").toByteArray());

	connect(ui.actionNew_Row, SIGNAL(triggered()),
			this, SLOT(addRow()));
	connect(ui.actionRemove_Row, SIGNAL(triggered()),
			this, SLOT(removeRow()));
	connect(ui.actionTruncate_Table, SIGNAL(triggered()),
			this, SLOT(truncateTable()));
	connect(ui.actionExport_Data, SIGNAL(triggered()),
			this, SLOT(exportData()));
	connect(ui.actionCommit, SIGNAL(triggered()),
			this, SLOT(commit()));
	connect(ui.actionRollback, SIGNAL(triggered()),
			this, SLOT(rollback()));
	connect(ui.actionRipOut, SIGNAL(triggered()),
			this, SLOT(openStandaloneWindow()));
	connect(ui.actionClose, SIGNAL(triggered()),
			this, SLOT(close()));
	connect(ui.action_Goto_Line, SIGNAL(triggered()),
			this, SLOT(gotoLine()));
	connect(keyPressEater, SIGNAL(copyRequest()),
			this, SLOT(copyHandler()));
// 	connect(parent, SIGNAL(prefsChanged()), ui.tableView, SLOT(repaint()));
	connect(ui.actionBLOB_Preview, SIGNAL(toggled(bool)),
			this, SLOT(handleBlobPreview(bool)));
	connect(ui.tabWidget, SIGNAL(currentChanged(int)),
			this, SLOT(tabWidget_currentChanged(int)));
	connect(ui.tableView->horizontalHeader(), SIGNAL(sectionResized(int, int, int)),
			this, SLOT(tableView_dataResized(int, int, int)));
	connect(ui.tableView->verticalHeader(), SIGNAL(sectionResized(int, int, int)),
			this, SLOT(tableView_dataResized(int, int, int)));
}

DataViewer::~DataViewer()
{
	QSettings settings("yarpen.cz", "sqliteman");
    settings.setValue("dataviewer/state", saveState());
}

bool DataViewer::setTableModel(QAbstractItemModel * model, bool showButtons)
{
	SqlTableModel * old = qobject_cast<SqlTableModel*>(ui.tableView->model());
	if (old && old->pendingTransaction())
	{
		int com = QMessageBox::question(this, tr("Sqliteman"),
										tr("There is a pending transaction in progress. Perform commit?"
										   "\n\nHelp:\nYes = commit\nNo = rollback"
										   "\nCancel = skip this operation and stay in the current table"),
										   QMessageBox::Yes, QMessageBox::No, QMessageBox::Cancel);
		if (com == QMessageBox::No)
			rollback();
		else if (com == QMessageBox::Cancel)
			return false;
		else
		{
			if (!old->submitAll())
			{
				int ret = QMessageBox::question(this, tr("Sqliteman"),
						tr("There is a pending transaction in progress. That cannot be commited now."\
						"\nError: %1\n"\
						"Perform rollback?").arg(old->lastError().text()),
						QMessageBox::Yes, QMessageBox::No);
				if(ret == QMessageBox::Yes)
					rollback();
				else
					return false;

			}
		}
	}

//	delete makes snapshot window empty
// 	delete(ui.tableView->model());
// 	delete(ui.tableView->selectionModel());
	ui.tableView->setModel(model);
	connect(ui.tableView->selectionModel(),
			SIGNAL(selectionChanged(const QItemSelection &, const QItemSelection &)),
			this,
			SLOT(tableView_selectionChanged(const QItemSelection &, const QItemSelection &)));
	ui.itemView->setModel(model);
	ui.tabWidget->setCurrentIndex(0);
	resizeViewToContents(model);
	setShowButtons(showButtons);
	
	QString cached;
	if (qobject_cast<QSqlQueryModel*>(model)->rowCount() != 0
		   && qobject_cast<QSqlQueryModel*>(model)->canFetchMore())
    {
		cached = DataViewer::canFetchMore();
    }
    else
        cached = "";

	setStatusText(tr("Query OK\nRow(s) returned: %1 %2").arg(model->rowCount()).arg(cached));

	return true;
}

void DataViewer::freeResources()
{
	QSqlQueryModel * m = qobject_cast<QSqlQueryModel*>(ui.tableView->model());
	if (m)
		m->clear();
}

void DataViewer::tableView_dataResized(int column, int oldWidth, int newWidth) 
{
	dataResized = true;
}

void DataViewer::resizeEvent(QResizeEvent * event)
{
	if (!dataResized && ui.tableView->model())
		resizeViewToContents(ui.tableView->model());
}


void DataViewer::resizeViewToContents(QAbstractItemModel * model)
{
	if (model->columnCount() <= 0)
		return;

	ui.tableView->resizeColumnsToContents();
	ui.tableView->resizeRowsToContents();

	int total = 0;
	for (int i = 0; i < model->columnCount(); ++i)
		total += ui.tableView->columnWidth(i);

	if (total < ui.tableView->viewport()->width()) 
	{
		int extra = (ui.tableView->viewport()->width() - total)
			/ model->columnCount();
		for (int i = 0; i < model->columnCount(); ++i)
			ui.tableView->setColumnWidth(i, ui.tableView->columnWidth(i) + extra);
	}
	dataResized = false;
}

void DataViewer::setStatusText(const QString & text)
{
	ui.statusText->setPlainText(text);
}

void DataViewer::appendStatusText(const QString & text)
{
	ui.statusText->append(text);
}

void DataViewer::showStatusText(bool show)
{
	(show) ? ui.statusText->show() : ui.statusText->hide();
}

void DataViewer::setShowButtons(bool show)
{
	ui.actionTruncate_Table->setEnabled(show && ui.tableView->model()->rowCount() > 0);
	ui.actionRemove_Row->setEnabled(show && ui.tableView->model()->rowCount() > 0);
	ui.actionNew_Row->setEnabled(show);
	ui.actionCommit->setEnabled(show);
	ui.actionRollback->setEnabled(show);
	ui.actionRipOut->setEnabled(ui.tableView->model()->rowCount() > 0);
	ui.actionExport_Data->setEnabled(ui.tableView->model()->rowCount() > 0);
}

void DataViewer::addRow()
{
	SqlTableModel * model = qobject_cast<SqlTableModel *>(ui.tableView->model());
	if(model)
	{
		model->insertRows(model->rowCount(), 1);
		ui.tableView->scrollToBottom();
		setShowButtons(true);
	}
}

void DataViewer::removeRow()
{
	SqlTableModel * model = qobject_cast<SqlTableModel *>(ui.tableView->model());
	if(model)
	{
		model->removeRows(ui.tableView->currentIndex().row(), 1);
		setShowButtons(true);
	}
}

void DataViewer::truncateTable()
{
	int ret = QMessageBox::question(this, tr("Sqliteman"),
					tr("Are you sure you want to remove all content from this table?"),
					QMessageBox::Yes, QMessageBox::No);
	if(ret == QMessageBox::No)
		return;

	SqlTableModel * model = qobject_cast<SqlTableModel *>(ui.tableView->model());
	if (!model)
		return;

	// prevent cached data when truncating the table
	if (model->pendingTransaction())
		rollback();
	while (model->canFetchMore())
		model->fetchMore();
	model->removeRows(0, model->rowCount());
}

void DataViewer::exportData()
{
	QString tmpTableName("<any_table>");
	SqlTableModel * m = qobject_cast<SqlTableModel*>(ui.tableView->model());
	if (m)
		tmpTableName = m->tableName();

	DataExportDialog *dia = new DataExportDialog(this, tmpTableName);
	if (dia->exec())
		if (!dia->doExport())
			QMessageBox::warning(this, tr("Export Error"), tr("Data export failed"));
	delete dia;
}

QSqlQueryModel* DataViewer::tableData()
{
	return qobject_cast<QSqlQueryModel *>(ui.tableView->model());
}

QStringList DataViewer::tableHeader()
{
	QStringList ret;
	QSqlQueryModel *q = qobject_cast<QSqlQueryModel *>(ui.tableView->model());

	for (int i = 0; i < q->columnCount() ; ++i)
		ret << q->headerData(i, Qt::Horizontal).toString();

	return ret;
}

void DataViewer::commit()
{
	// HACK: some Qt4 versions crash on commit/rollback when there
	// is a new - currently edited - row in a transaction. This
	// forces to close the editor/delegate.
	ui.tableView->selectRow(ui.tableView->currentIndex().row());
	SqlTableModel * model = qobject_cast<SqlTableModel *>(ui.tableView->model());
	if (!model->submitAll())
	{
		int ret = QMessageBox::question(this, tr("Sqliteman"),
				tr("There is a pending transaction in progress. That cannot be commited now."\
				   "\nError: %1\n"\
				   "Perform rollback?").arg(model->lastError().text()),
				QMessageBox::Yes, QMessageBox::No);
		if(ret == QMessageBox::Yes)
			rollback();
		return;
	}
	model->setPendingTransaction(false);
	resizeViewToContents(model);
	setShowButtons(true);
}

void DataViewer::rollback()
{
	// HACK: some Qt4 versions crash on commit/rollback when there
	// is a new - currently edited - row in a transaction. This
	// forces to close the editor/delegate.
	ui.tableView->selectRow(ui.tableView->currentIndex().row());
	SqlTableModel * model = qobject_cast<SqlTableModel *>(ui.tableView->model());
	model->revertAll();
	model->setPendingTransaction(false);
	resizeViewToContents(model);
	setShowButtons(true);
}

void DataViewer::copyHandler()
{
	QItemSelectionModel *selectionModel = ui.tableView->selectionModel();
	QModelIndexList selectedIndexes = selectionModel->selectedIndexes();
	QModelIndex index;
	// This looks very "pythonic" maybe there is better way to do...
	QMap<int,QMap<int,QString> > snapshot;
	QStringList out;

	foreach (index, selectedIndexes)
		snapshot[index.row()][index.column()] = index.data().toString();
	
	QMapIterator<int,QMap<int,QString> > it(snapshot);
	while (it.hasNext())
	{
		it.next();
		QMapIterator<int,QString> j(it.value());
		while (j.hasNext())
		{
			j.next();
			out << j.value();
			if (j.hasNext())
				out << "\t";
		}
		out << "\n";
	}

	if (out.size() != 0)
		QApplication::clipboard()->setText(out.join(QString::null));
}

void DataViewer::openStandaloneWindow()
{
	SqlQueryModel *qm;
	SqlTableModel *tm = qobject_cast<SqlTableModel*>(ui.tableView->model());

#ifdef WIN32
    // win windows are always top when there is this parent
    DataViewer *w = new DataViewer(0);
#else
    DataViewer *w = new DataViewer(this);
#endif
	w->setAttribute(Qt::WA_DeleteOnClose);

	//! TODO: change setWindowTitle() to the unified QString().arg() sequence aftre string unfreezing
	if (tm)
	{
		w->setWindowTitle(tm->tableName() + " - "
				+ QDateTime::currentDateTime().toString() + " - " 
				+ tr("Data Snapshot"));
		qm = new SqlQueryModel(w);
		qm->setQuery(QString("select * from \"%1\".\"%2\";").arg(tm->schema()).arg(tm->tableName()),
					QSqlDatabase::database(SESSION_NAME));
	}
	else
	{
		w->setWindowTitle("SQL - "
				+ QDateTime::currentDateTime().toString() + " - " 
				+ tr("Data Snapshot"));
		qm = qobject_cast<SqlQueryModel*>(ui.tableView->model());
	}

	w->setTableModel(qm);
	w->ui.statusText->setText(tr("%1 snapshot for: %2")
								.arg("<tt>"+QDateTime::currentDateTime().toString()+"</tt><br/>")
								.arg("<br/><tt>" + qm->query().lastQuery())+ "</tt>");
	w->ui.mainToolBar->hide();
	w->ui.snapshotToolBar->hide();
	w->ui.actionClose->setVisible(true);
	w->ui.tabWidget->removeTab(2);
	w->show();
}

void DataViewer::handleBlobPreview(bool state)
{
	ui.blobPreviewBox->setVisible(state);
	if (state)
		tableView_selectionChanged(QItemSelection(), QItemSelection());
}

void DataViewer::tableView_selectionChanged(const QItemSelection &, const QItemSelection &)
{
	SqlTableModel *tm = qobject_cast<SqlTableModel*>(ui.tableView->model());
    bool enable = (tm != 0);
    actInsertNull->setEnabled(enable);
    actOpenEditor->setEnabled(enable);

    if (ui.blobPreviewBox->isVisible())
    {
	    ui.blobPreview->setBlobData(ui.tableView->model()->data(ui.tableView->currentIndex(),
		    													Qt::EditRole)
			    				   );
    }
	
}
void DataViewer::tabWidget_currentChanged(int ix)
{
	if (ix == 0)
	{
		// be carefull with this. See itemView_indexChanged() docs.
		disconnect(ui.itemView, SIGNAL(indexChanged()),
				this, SLOT(itemView_indexChanged()));
	}
	if (ix == 1)
	{
		ui.itemView->setCurrentIndex(ui.tableView->currentIndex().row(),
									 ui.tableView->currentIndex().column());
		// be carefull with this. See itemView_indexChanged() docs.
		connect(ui.itemView, SIGNAL(indexChanged()),
				this, SLOT(itemView_indexChanged()));
	}
	
	if (ui.actionBLOB_Preview->isChecked())
		ui.blobPreviewBox->setVisible(ix!=2);
	ui.statusText->setVisible(ix != 2);
	ui.action_Goto_Line->setEnabled(ix!=2);
}

void DataViewer::itemView_indexChanged()
{
	ui.tableView->setCurrentIndex(
		ui.tableView->model()->index(ui.itemView->currentIndex(),
								     ui.itemView->currentColumn())
							);
}

void DataViewer::showSqlScriptResult(QString line)
{
	ui.scriptEdit->append(line);
	ui.scriptEdit->append("\n");
	ui.scriptEdit->ensureLineVisible(ui.scriptEdit->lines());
	ui.tabWidget->setCurrentIndex(2);
	setShowButtons(false);
}

void DataViewer::sqlScriptStart()
{
	ui.scriptEdit->clear();
}

const QString DataViewer::canFetchMore()
{
	return tr("(More rows can be fetched. Scroll the resultset for more rows and/or read the documentation.)");
}

void DataViewer::gotoLine()
{
	bool ok;
	int row = QInputDialog::getInt(this, tr("Goto Line"), tr("Goto Line:"),
								   ui.tableView->currentIndex().row(), // value
								   1, // min
								   ui.tableView->model()->rowCount(), // max (no fetchMore loop)
								   1, // step
								   &ok);
	if (!ok)
		return;

	QModelIndex left;
	SqlTableModel * model = qobject_cast<SqlTableModel *>(ui.tableView->model());
	int column = ui.tableView->currentIndex().isValid() ? ui.tableView->currentIndex().column() : 0;
	row -= 1;

	if (model)
		left = model->createIndex(row, column);
	else
	{
		SqlQueryModel * model = qobject_cast<SqlQueryModel *>(ui.tableView->model());
		if (model)
			left = model->createIndex(row, column);
	}

	ui.tableView->selectionModel()->select(QItemSelection(left, left),
										   QItemSelectionModel::ClearAndSelect);
	ui.tableView->setCurrentIndex(left);
}

void DataViewer::actOpenEditor_triggered()
{
    ui.tableView->edit(ui.tableView->currentIndex());
}

void DataViewer::actInsertNull_triggered()
{
    ui.tableView->model()->setData(ui.tableView->currentIndex(), QString(), Qt::EditRole); 
}


/* Tools *************************************************** */

bool DataViewerTools::KeyPressEater::eventFilter(QObject *obj, QEvent *event)
{
	if (event->type() == QEvent::KeyPress)
	{
		QKeyEvent *keyEvent = static_cast<QKeyEvent *>(event);
		if (keyEvent == QKeySequence::Copy)
		{
			emit copyRequest();
			return true;
		}
		return QObject::eventFilter(obj, event);
	}
	else
	{
		// standard event processing
		return QObject::eventFilter(obj, event);
	}
}


