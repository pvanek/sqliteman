/*
For general Sqliteman copyright and licensing information please refer
to the COPYING file provided with the program. Following this notice may exist
a copyright and/or license notice that predates the release of Sqliteman
for which a new license (GPL+exception) is in place.
*/
#include "sqlparser.h"


SqlEditorTools::SqlParser::SqlParser(QString text)
	: QObject(),
	m_update(false)
{
	m_lines = text.split('\n');
	m_string = text;
}

QString SqlEditorTools::SqlParser::getStatement(int cursorPosition)
{
	int currPosition = -1;
	int start = 0;
	int end = 0;
	ParserState state = Whitespace;
	bool started = false;
	QChar next;

	for (int line = 0; line < m_lines.count(); ++line)
	{
		QString currLine = m_lines.at(line);

		for (int column = 0; column < currLine.length(); ++column)
		{
			++currPosition;
			next = (column < currLine.length()-1) ? currLine.at(column+1) : ' ';
			// strings
			if (state == Whitespace && currLine.at(column) == '\'')
			{
				state = String;
				continue;
			}
			if (state == String && currLine.at(column) == '\'')
			{
				state = Whitespace;
				continue;
			}
			// comments
			if (state == Whitespace && currLine.at(column) == '-' && next == '-')
			{
				// line comment - rest is ignored
				currPosition += currLine.length() - column - 1;
				break;
			}
			if (state == Whitespace && currLine.at(column) == '/' && next == '*')
			{
				state = Comment;
				continue;
			}
			if (state == Comment && currLine.at(column) == '*' && next == '/')
			{
				state = Whitespace;
				continue;
			}
			// start of commands
			if (state == Whitespace && !started
						 && (currLine.mid(column, 6).toUpper() == "CREATE"
						 || currLine.mid(column, 5).toUpper() == "ALTER"
						 || currLine.mid(column, 4).toUpper() == "DROP"
						 || currLine.mid(column, 6).toUpper() == "SELECT"
						 || currLine.mid(column, 6).toUpper() == "PRAGMA"
						 || currLine.mid(column, 6).toUpper() == "COMMIT"
						 || currLine.mid(column, 8).toUpper() == "ROLLBACK"
						 || currLine.mid(column, 5).toUpper() == "BEGIN"
						 || currLine.mid(column, 6).toUpper() == "VACUUM"
						 || currLine.mid(column, 6).toUpper() == "ATTACH"
						 || currLine.mid(column, 6).toUpper() == "DETACH"
						 || currLine.mid(column, 7).toUpper() == "ANALYZE"
						 || currLine.mid(column, 7).toUpper() == "REINDEX"
						 || currLine.mid(column, 6).toUpper() == "INSERT"
						 || currLine.mid(column, 6).toUpper() == "UPDATE"
						 || currLine.mid(column, 6).toUpper() == "DELETE"
						 || currLine.mid(column, 7).toUpper() == "REPLACE")
			   )
			{
				started = true;
				if (currPosition > cursorPosition)
				{
					return "";//"start overflow";
				}
				start = currPosition;
				end = 0;
				continue;
			}
			if (state == Whitespace && started && currLine.at(column) == ';')
			{
				started = false;
				end = currPosition;
			}

			if (start <= cursorPosition && cursorPosition <= end)
			{
				return m_string.mid(start, end-start /*+ line*/).trimmed();
			}
		}
		++currPosition;
	}
	if (started)
	{
		return m_string.mid(start, m_string.length()-start);
	}
	return QString("SQL statement not found. Pos: %1. Start: %2. End: %3.").arg(cursorPosition).arg(start).arg(end);
}

bool SqlEditorTools::SqlParser::updateTree(const QString & sql)
{
	if (sql.isNull())
		return false;
	QString tmp(sql.trimmed().toUpper());
	if (tmp.left(4) == "DROP" || tmp.left(6) == "CREATE" || tmp.left(5) == "ALTER")
		return true;
	return false;
}


#ifdef PARSERDEBUG
#include <QApplication>
int main(int argc, char ** argv)
{
	QApplication a(argc, argv);
	SqlParser s("select * from foo;\nselect * from bar;");

	qDebug() << s.getStatement(20);
	return 0;
}

#endif
