/*
For general Sqliteman copyright and licensing information please refer
to the COPYING file provided with the program. Following this notice may exist
a copyright and/or license notice that predates the release of Sqliteman
for which a new license (GPL+exception) is in place.
*/
#include <QProcess>
#include <QtDebug>

#include "sqliteprocess.h"
#include "litemanwindow.h"


SqliteProcess::SqliteProcess(QObject * parent)
	: QObject(parent),
	  m_error(""),
	  m_success(false)
{
	m_mainDbPath = dynamic_cast<LiteManWindow*>(parent)->mainDbPath();
}

void SqliteProcess::setStandardOutputFile(const QString & fname)
{
	m_stdout = fname;
}

void SqliteProcess::start(const QStringList & commands, const QStringList & options)
{
	QProcess p;

	if (!m_stdout.isNull())
		p.setStandardOutputFile(m_stdout);

	QStringList list = QStringList() << options << m_mainDbPath << commands;
	qDebug() << "Running " << SQLITE_BINARY << " with args: " << list;
	qDebug() << QString("     %1 %2").arg(SQLITE_BINARY).arg(list.join(" "));
	p.start(SQLITE_BINARY, list);
	if (!p.waitForStarted() || !p.waitForFinished(-1) || (p.exitStatus() != QProcess::NormalExit))
	{
		m_success = false;
		switch (p.error())
		{
			case (QProcess::FailedToStart) :
				m_error = tr("The process failed to start. Either the invoked program is missing, or you may have insufficient permissions to invoke the program.");
				break;
			case (QProcess::Crashed) :
				m_error = tr("The process crashed some time after starting successfully.");
				break;
			case (QProcess::WriteError) :
				m_error = tr("An error occurred when attempting to write to the process. For example, the process may not be running, or it may have closed its input channel.");
				break;
			case (QProcess::ReadError):
				m_error = tr("An error occurred when attempting to read from the process. For example, the process may not be running.");
				break;
			default:
				m_error = tr("An unknown error occurred.");
		}
	}
	else
		m_success = true;
	m_stderr = QString(p.readAllStandardError());
	m_stdout = QString(p.readAllStandardOutput());
}
