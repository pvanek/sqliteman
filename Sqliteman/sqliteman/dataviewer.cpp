/*
For general Sqliteman copyright and licensing information please refer
to the COPYING file provided with the program. Following this notice may exist
a copyright and/or license notice that predates the release of Sqliteman
for which a new license (GPL+exception) is in place.
*/

#include <QMessageBox>
#include <QSqlTableModel>
#include <QSqlQuery>
#include <QSqlError>
#include <QKeyEvent>
#include <QClipboard>

#include "dataviewer.h"
#include "dataexportdialog.h"


DataViewer::DataViewer(QWidget * parent) : QMainWindow(parent)
{
	ui.setupUi(this);
	ui.splitter->setCollapsible(0, false);
	ui.splitter->setCollapsible(1, false);
	ui.actionNew_Row->setIcon(QIcon(QString(ICON_DIR) + "/insert_table_row.png"));
	ui.actionRemove_Row->setIcon(QIcon(QString(ICON_DIR) + "/delete_table_row.png"));
	ui.actionTruncate_Table->setIcon(QIcon(QString(ICON_DIR) + "/clear_table_contents.png"));
	ui.actionCommit->setIcon(QIcon(QString(ICON_DIR) + "/database_commit.png"));
	ui.actionRollback->setIcon(QIcon(QString(ICON_DIR) + "/database_rollback.png"));

	// workaround for Ctrl+C
	DataViewerTools::KeyPressEater *keyPressEater = new DataViewerTools::KeyPressEater(this);
	ui.tableView->installEventFilter(keyPressEater);

	connect(ui.actionNew_Row, SIGNAL(triggered()), this, SLOT(addRow()));
	connect(ui.actionRemove_Row, SIGNAL(triggered()), this, SLOT(removeRow()));
	connect(ui.actionTruncate_Table, SIGNAL(triggered()), this, SLOT(truncateTable()));
	connect(ui.actionExport_Data, SIGNAL(triggered()), this, SLOT(exportData()));
	connect(ui.actionCommit, SIGNAL(triggered()), this, SLOT(commit()));
	connect(ui.actionRollback, SIGNAL(triggered()), this, SLOT(rollback()));
	connect(keyPressEater, SIGNAL(copyRequest()), this, SLOT(copyHandler()));
// 	connect(parent, SIGNAL(prefsChanged()), ui.tableView, SLOT(repaint()));
}

bool DataViewer::setTableModel(QAbstractItemModel * model)
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
	ui.tableView->resizeRowsToContents();
	ui.tableView->resizeColumnsToContents();
	return true;
}

void DataViewer::setStatusText(const QString & text)
{
	ui.statusText->setPlainText(text);
}

void DataViewer::showStatusText(bool show)
{
	(show) ? ui.statusText->show() : ui.statusText->hide();
}

void DataViewer::showButtons(bool show)
{
	ui.toolBar->setEnabled(show);
// 	ui.actionTruncate_Table->setEnabled(show);
// 	ui.actionRemove_Row->setEnabled(show);
// 	ui.actionNew_Row->setEnabled(show);
// 	ui.actionExport_Data->setEnabled(true);
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
	DataExportDialog *dia = new DataExportDialog(this);
	if (dia->exec())
		if (!dia->doExport())
			QMessageBox::warning(this, tr("Export Error"), tr("Data export failed"));
	delete dia;
}

SqlTableModel* DataViewer::tableData()
{
	return qobject_cast<SqlTableModel *>(ui.tableView->model());
}

QStringList DataViewer::tableHeader()
{
	QStringList ret;
	SqlTableModel *q = qobject_cast<SqlTableModel *>(ui.tableView->model());

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
