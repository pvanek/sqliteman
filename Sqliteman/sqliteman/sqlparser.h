/*
For general Sqliteman copyright and licensing information please refer
to the COPYING file provided with the program. Following this notice may exist
a copyright and/or license notice that predates the release of Sqliteman
for which a new license (GPL+exception) is in place.
*/
#ifndef SQLPARSER_H
#define SQLPARSER_H

#include <QStringList>


namespace SqlEditorTools
{

	/*! \brief Very simple and stupid SQL parser.
	It tries to find start and end of the SQL statement containing
	text cursor in the text editor.
	\author Petr Vanek <petr@scribus.info>
	*/
	class SqlParser : public QObject
	{
		Q_OBJECT

		public:
			SqlParser(QString text);
			~SqlParser(){}

			QString getStatement(int cursorPosition);

			static bool updateTree(const QString & sql);
		private:
			enum ParserState {Whitespace, Comment, String};
			QStringList m_lines;
			QString m_string;
			bool m_update;
	};

};

#endif
