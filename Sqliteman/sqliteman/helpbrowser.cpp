/*
For general Sqliteman copyright and licensing information please refer
to the COPYING file provided with the program. Following this notice may exist
a copyright and/or license notice that predates the release of Sqliteman
for which a new license (GPL+exception) is in place.
*/
#include <QUrl>
#include <QDir>
#include <QSettings>

#include "helpbrowser.h"


HelpBrowser::HelpBrowser(const QString & lang, QWidget * parent)
	: QMainWindow(parent)
{
	ui.setupUi(this);
	// menu
	QStringList spaths;

	QString docs(QString("%1%2").arg(DOC_DIR).arg(lang));
	if (!lang.isNull() && !lang.isEmpty() && QDir().exists(docs))
		spaths << docs;
	else
		spaths << QString("%1en").arg(DOC_DIR);

	ui.menuBrowser->setSearchPaths(spaths);
	ui.textBrowser->setSearchPaths(spaths);
	ui.menuBrowser->setSource(QUrl("menu.html"));
	ui.textBrowser->setSource(QUrl("index.html"));

	// settings
	QSettings settings("yarpen.cz", "sqliteman");
	restoreGeometry(settings.value("help/geometry").toByteArray());
	ui.splitter->restoreState(settings.value("help/splitter").toByteArray());

	setHistoryButtonsState();

	connect(ui.menuBrowser, SIGNAL(anchorClicked(const QUrl &)),
			this, SLOT(menuBrowser_anchorClicked(const QUrl &)));
	connect(ui.actionBack, SIGNAL(triggered()), this, SLOT(backward()));
	connect(ui.actionForward, SIGNAL(triggered()), this, SLOT(forward()));
	connect(ui.action_Close, SIGNAL(triggered()), this, SLOT(close()));
}

void HelpBrowser::closeEvent(QCloseEvent *e)
{
	QSettings settings("yarpen.cz", "sqliteman");

	settings.setValue("help/geometry", saveGeometry());
	settings.setValue("help/splitter", ui.splitter->saveState());

	QMainWindow::closeEvent(e);
}

void HelpBrowser::menuBrowser_anchorClicked(const QUrl & url)
{
	ui.textBrowser->setSource(url);
	ui.menuBrowser->setSource(QUrl("menu.html"));
	setHistoryButtonsState();
}

void HelpBrowser::setHistoryButtonsState()
{
	ui.actionBack->setEnabled(ui.textBrowser->isBackwardAvailable());
	ui.actionForward->setEnabled(ui.textBrowser->isForwardAvailable());
}

void HelpBrowser::forward()
{
	ui.textBrowser->forward();
	setHistoryButtonsState();
}

void HelpBrowser::backward()
{
	ui.textBrowser->backward();
	setHistoryButtonsState();
}
