/*
For general Sqliteman copyright and licensing information please refer
to the COPYING file provided with the program. Following this notice may exist
a copyright and/or license notice that predates the release of Sqliteman
for which a new license (GPL+exception) is in place.
*/

#ifndef SQLITEPROCESS_H
#define SQLITEPROCESS_H

#include <QStringList>


class SqliteProcess : public QObject
{
	Q_OBJECT

	public:
		SqliteProcess(QObject * parent = 0);

		void start(const QStringList & commands, const QStringList & options = QStringList());
		QString errorMessage() { return m_error; };
		bool success() { return m_success; };

		void setStandardOutputFile(const QString & fname);

		QString allStderr() { return m_stderr; };
		QString allStdout() { return m_stdout; };

	private:
		QString m_mainDbPath;
		QString m_error;
		bool m_success;
		QString m_stdout;
		QString m_stderr;
};

#endif
