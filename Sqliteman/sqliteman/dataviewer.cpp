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

#include "dataviewer.h"
#include "dataexportdialog.h"
#include "sqlmodels.h"
#include "database.h"
#include "sqldelegate.h"
#include "utils.h"
#include "blobpreviewwidget.h"


DataViewer::DataViewer(QWidget * parent)
	: QMainWindow(parent)
{
	ui.setupUi(this);
	handleBlobPreview(false);

	ui.splitter->setCollapsible(0, false);
	ui.splitter->setCollapsible(1, false);
	ui.actionNew_Row->setIcon(getIcon("insert_table_row.png"));
	ui.actionRemove_Row->setIcon(getIcon("delete_table_row.png"));
	ui.actionTruncate_Table->setIcon(getIcon("clear_table_contents.png"));
	ui.actionCommit->setIcon(getIcon("database_commit.png"));
	ui.actionRollback->setIcon(getIcon("database_rollback.png"));

	// custom delegate
	ui.tableView->setItemDelegate(new SqlDelegate(this));

	// workaround for Ctrl+C
	DataViewerTools::KeyPressEater *keyPressEater = new DataViewerTools::KeyPressEater(this);
	ui.tableView->installEventFilter(keyPressEater);

	connect(ui.actionNew_Row, SIGNAL(triggered()), this, SLOT(addRow()));
	connect(ui.actionRemove_Row, SIGNAL(triggered()), this, SLOT(removeRow()));
	connect(ui.actionTruncate_Table, SIGNAL(triggered()), this, SLOT(truncateTable()));
	connect(ui.actionExport_Data, SIGNAL(triggered()), this, SLOT(exportData()));
	connect(ui.actionCommit, SIGNAL(triggered()), this, SLOT(commit()));
	connect(ui.actionRollback, SIGNAL(triggered()), this, SLOT(rollback()));
	connect(ui.actionRipOut, SIGNAL(triggered()), this, SLOT(openStandaloneWindow()));
	connect(ui.actionClose, SIGNAL(triggered()), this, SLOT(close()));
	connect(keyPressEater, SIGNAL(copyRequest()), this, SLOT(copyHandler()));
// 	connect(parent, SIGNAL(prefsChanged()), ui.tableView, SLOT(repaint()));
	connect(ui.actionBLOB_Preview, SIGNAL(toggled(bool)), this, SLOT(handleBlobPreview(bool)));
	connect(ui.tabWidget, SIGNAL(currentChanged(int)), this, SLOT(tabWidget_currentChanged(int)));
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
		{
			rollback();
		}
		else if (com == QMessageBox::Cancel)
			return false;
		else
		{
			if (!old->submitAll())
			{
				int ret = QMessageBox::question(this, tr("Sqliteman"),
						tr("There is a pending transaction in progress. That cannot be commited now."\
						"\nError: %1\n"\
						"Perform rollback?").arg(old->lastError().databaseText()),
						QMessageBox::Yes, QMessageBox::No);
				if(ret == QMessageBox::Yes)
					rollback();
				else
					return false;
			}
		}
	}
	ui.tableView->setModel(model);
	connect(ui.tableView->selectionModel(), SIGNAL(selectionChanged(const QItemSelection &, const QItemSelection &)),
			this, SLOT(tableView_selectionChanged(const QItemSelection &, const QItemSelection &)));
	ui.itemView->setModel(model);
	resizeViewToContents(model);
	setShowButtons(showButtons);
	
	QString cached;
	if (qobject_cast<QSqlQueryModel*>(model)->canFetchMore())
		cached = tr("(more rows can be fetched)");
	setStatusText(tr("Query OK\nRow(s) returned: %1 %2").arg(model->rowCount()).arg(cached));

	return true;
}

void DataViewer::resizeViewToContents(QAbstractItemModel * model)
{
	ui.tableView->resizeColumnsToContents();
	ui.tableView->resizeRowsToContents();
	for (int i = 0; i < model->columnCount(); ++i)
	{
		if (ui.tableView->columnWidth(i) < 150)
			ui.tableView->setColumnWidth(i, 150);
	}
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
	ui.actionTruncate_Table->setEnabled(show);
	ui.actionRemove_Row->setEnabled(show);
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
		model->insertRows(model->rowCount(), 1);
}

void DataViewer::removeRow()
{
	SqlTableModel * model = qobject_cast<SqlTableModel *>(ui.tableView->model());
	if(model)
		model->removeRows(ui.tableView->currentIndex().row(), 1);
}

void DataViewer::truncateTable()
{
	int ret = QMessageBox::question(this, tr("Sqliteman"),
					tr("Are you sure you want to remove all content from this table?"),
					QMessageBox::Yes, QMessageBox::No);
	if(ret == QMessageBox::No)
		return;
	SqlTableModel * model = qobject_cast<SqlTableModel *>(ui.tableView->model());
	if(model)
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
				   "Perform rollback?").arg(model->lastError().databaseText()),
				QMessageBox::Yes, QMessageBox::No);
		if(ret == QMessageBox::Yes)
			rollback();
		return;
	}
	model->setPendingTransaction(false);
	resizeViewToContents(model);
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
	QString table;
	SqlQueryModel *qm;
	SqlTableModel *tm = qobject_cast<SqlTableModel*>(ui.tableView->model());

	DataViewer *w = new DataViewer(this);
	w->setAttribute(Qt::WA_DeleteOnClose);
	w->setWindowTitle(tr("Data Snapshot"));

	if (tm)
	{
		qm = new SqlQueryModel(w);
		qm->setQuery(QString("select * from \"%1\".\"%2\";").arg(tm->schema()).arg(tm->tableName()),
					QSqlDatabase::database(SESSION_NAME));
	}
	else
		qm = qobject_cast<SqlQueryModel*>(ui.tableView->model());

	w->setTableModel(qm);
	w->ui.statusText->setText(tr("%1 snapshot for: %2")
								.arg("<tt>"+QDateTime::currentDateTime().toString()+"</tt><br/>")
								.arg("<br/><tt>" + qm->query().lastQuery())+ "</tt>");
	w->ui.mainToolBar->hide();
	w->ui.snapshotToolBar->hide();
	w->ui.actionClose->setVisible(true);
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
	if (!ui.blobPreviewBox->isVisible())
		return;
	ui.blobPreview->setBlobData(ui.tableView->model()->data(ui.tableView->currentIndex(),
															Qt::EditRole)
							   );
	
}

void DataViewer::tabWidget_currentChanged(int ix)
{
	if (ix == 0)
	{
		QModelIndex mi = ui.tableView->currentIndex().sibling(ui.itemView->currentIndex(), 0);
		ui.tableView->setCurrentIndex(mi);
	}
	else
		ui.itemView->setCurrentIndex(ui.tableView->currentIndex().row());
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
