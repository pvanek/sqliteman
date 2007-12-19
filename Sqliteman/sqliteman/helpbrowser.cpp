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
#include "utils.h"


HelpBrowser::HelpBrowser(const QString & lang, QWidget * parent)
#ifdef WIN32
    // win windows are always top when there is this parent
    : QMainWindow(0)
#else
	: QMainWindow(parent)
#endif
{
	ui.setupUi(this);
	ui.actionBack->setIcon(Utils::getIcon("go-previous.png"));
	ui.actionForward->setIcon(Utils::getIcon("go-next.png"));
	ui.action_Close->setIcon(Utils::getIcon("close.png"));
	// menu
	QStringList spaths;

	QString docs(QString(DOC_DIR) + "%1/");
	if (!lang.isNull() && !lang.isEmpty() && QDir().exists(docs))
		docs = docs.arg(lang);
	else
		docs = docs.arg("en");

#ifdef WANT_RESOURCES
	docs = "qrc" + docs;
#endif

	spaths << docs;

	ui.textBrowser->setSearchPaths(spaths);
	ui.textBrowser->setSource(QUrl(docs + "index.html"));

	// settings
	QSettings settings("yarpen.cz", "sqliteman");
	restoreGeometry(settings.value("help/geometry").toByteArray());
	ui.splitter->restoreState(settings.value("help/splitter").toByteArray());

	ui.actionBack->setEnabled(false);
	ui.actionForward->setEnabled(false);

	connect(ui.actionBack, SIGNAL(triggered()), this, SLOT(backward()));
	connect(ui.actionForward, SIGNAL(triggered()), this, SLOT(forward()));
	connect(ui.action_Close, SIGNAL(triggered()), this, SLOT(close()));
	connect(ui.textBrowser, SIGNAL(backwardAvailable(bool)),
			ui.actionBack, SLOT(setEnabled(bool)));
	connect(ui.textBrowser, SIGNAL(forwardAvailable(bool)),
			ui.actionForward, SLOT(setEnabled(bool)));
}

void HelpBrowser::closeEvent(QCloseEvent *e)
{
	QSettings settings("yarpen.cz", "sqliteman");

	settings.setValue("help/geometry", saveGeometry());
	settings.setValue("help/splitter", ui.splitter->saveState());

	QMainWindow::closeEvent(e);
}

void HelpBrowser::forward()
{
	ui.textBrowser->forward();
}

void HelpBrowser::backward()
{
	ui.textBrowser->backward();
}
