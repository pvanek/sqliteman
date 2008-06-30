/*
For general Sqliteman copyright and licensing information please refer
to the COPYING file provided with the program. Following this notice may exist
a copyright and/or license notice that predates the release of Sqliteman
for which a new license (GPL+exception) is in place.

This file is based on the TOra source code. http://tora.sf.net
*/
/*****
*
* TOra - An Oracle Toolkit for DBA's and developers
* Copyright (C) 2003-2005 Quest Software, Inc
* Portions Copyright (C) 2005 Other Contributors
*
* This program is free software; you can redistribute it and/or
* modify it under the terms of the GNU General Public License
* as published by the Free Software Foundation;  only version 2 of
* the License is valid for this program.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with this program; if not, write to the Free Software
* Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
*
*	  As a special exception, you have permission to link this program
*	  with the Oracle Client libraries and distribute executables, as long
*	  as you follow the requirements of the GNU GPL in regard to all of the
*	  software in the executable aside from Oracle client libraries.
*
*	  Specifically you are not permitted to link this program with the
*	  Qt/UNIX, Qt/Windows or Qt Non Commercial products of TrollTech.
*	  And you are not permitted to distribute binaries compiled against
*	  these libraries without written consent from Quest Software, Inc.
*	  Observe that this does not disallow linking to the Qt Free Edition.
*
*	  You may link this product with any GPL'd Qt library such as Qt/Free
*
* All trademarks belong to their respective owners.
*
*****/
#include <QApplication>
#include <QMessageBox>

#include "tosqlparse.h"
#include "sqleditorwidget.h"
#include "sqlkeywords.h"

// FIXME: Prefs!
#define QUOTE_CHARACTER '\"'
#define DEF_TAB_WIDTH 4
// #define TOPARSE_DEBUG


inline bool toIsIdent(QChar c)
{
	return c.isLetterOrNumber() || c == '_' || c == '%' || c == '$' || c == '#';
}

/** Pop the last value out of a list.
 * @param lst List to pop value from (Also modified).
 * @return The value in the list of objects.
 */
template <class T>
		T toPop(std::list<T> &lst)
{
	if (lst.begin() == lst.end())
	{
		T ret;
		return ret;
	}
	T ret = (*lst.rbegin());
	lst.pop_back();
	return ret;
}


toSQLParse::statement::statement(type ntype, const QString &token, int cline)
		: Type(ntype), String(token), Line(cline)
{
	SubTokens = NULL;
}

std::list<toSQLParse::statement> &toSQLParse::statement::subTokens(void)
{
	if (!SubTokens)
		SubTokens = new std::list<statement>;
	return *SubTokens;
}

toSQLParse::statement::~statement()
{
	delete SubTokens;
}

toSQLParse::statement::statement(const statement &stat)
{
	Type = stat.Type;
	String = stat.String;
	Comment = stat.Comment;
	Line = stat.Line;
	if (stat.SubTokens)
	{
		SubTokens = new std::list<statement>;
		(*SubTokens) = (*stat.SubTokens);
	}
	else
		SubTokens = NULL;
}

const toSQLParse::statement &toSQLParse::statement::operator = (const statement & stat)
{
	Type = stat.Type;
	String = stat.String;
	Comment = stat.Comment;
	Line = stat.Line;
	delete SubTokens;
	if (stat.SubTokens)
	{
		SubTokens = new std::list<statement>;
		(*SubTokens) = (*stat.SubTokens);
	}
	else
		SubTokens = NULL;
	return *this;
}

bool toSQLParse::statement::operator == (const statement &stat) const
{
	if (Type != stat.Type ||
			Comment != stat.Comment ||
			String != stat.String)
		return false;
	if (SubTokens)
	{
		if (!stat.SubTokens && SubTokens->begin() != SubTokens->end())
			return false;
		if ((*SubTokens) != (*stat.SubTokens))
			return false;
	}
	else if (stat.SubTokens && stat.SubTokens->begin() != stat.SubTokens->end())
		return false;
	return true;
}

static const char *Operators[] =
	{":=",
	 "=>",
	 "||",
	 "**",
	 "<<",
	 ">>",
	 "..",
	 "<>",
	 "!=",
	 "~=",
	 "^=",
	 "<=",
	 ">=",
	 NULL
	};

QString toSQLParse::stringTokenizer::getToken(bool forward, bool comments)
{
	QChar c;
	QChar nc;
	QChar endString;

	enum
	{
		space,
		any,
		identifier,
		string,
		comment,
		label,
		bindOpen,
		bindClose
	} state = space;

	QString token;

	int inc = forward ? 1 : -1;

	while ((forward && Offset < int(String.length())) || (!forward && Offset >= 1))
	{
		if (!forward)
			Offset--;
		c = String[Offset];
		if (c == '\n')
			Line++;
		if ((forward && Offset < int(String.length() - 1)) || (!forward && Offset > 0))
			nc = String[Offset + inc];
		else
			nc = '\n';
		if (state == space)
		{
			if (forward && c == '-' && nc == '-')
			{
				int spos = Offset;
				if (forward)
					for (Offset++;Offset < int(String.length()) && String[Offset] != '\n';Offset++)
						;
				if (comments)
					return String.mid(spos, Offset - spos);
				continue;
			}
			if (c == '/' && nc == '*')
				state = comment;
			else if ((forward && c == '<' && nc == '<') ||
					 (!forward && c == '>' && nc == '>'))
				state = label;
			else if (!c.isSpace())
				state = any;
		}

		if (forward)
			Offset++;

		if (state != space)
		{
			if (forward)
				token += c;
			else
				token.prepend(c);
			switch (state)
			{
			case comment:
				if (c == '*' && nc == '/')
				{
					if (forward)
						token += nc;
					else
						token.prepend(nc);
					Offset += inc;
					if (comments)
						return token;
					else
					{
						state = space;
						token = QString::null;
					}
				}
				break;
			case label:
				if ((forward && c == '>' && nc == '>') ||
						(!forward && c == '<' && nc == '<'))
				{
					if (forward)
						token += nc;
					else
						token.prepend(nc);
					Offset += inc;
					return token;
				}
				break;
			case space:
				break;
			case bindOpen:
				if (!toIsIdent(nc))
				{
					if (nc == '<')
						state = bindClose;
					else
						return token;
				}
				break;
			case bindClose:
				if (c == '>')
					return token;
				break;
			case any:
				if (c == ':' && toIsIdent(nc))
				{
					state = bindOpen;
				}
				else if (toIsIdent(c))
				{
					if (!toIsIdent(nc))
						return token;
					state = identifier;
				}
				else if (c == '\'' || c == QUOTE_CHARACTER) // SQLITEMAN c == analyzer().quoteCharacter())
				{
					endString = c;
					state = string;
				}
				else
				{
					for (int i = 0;Operators[i];i++)
					{
						if ((forward && c == Operators[i][0] && nc == Operators[i][1]) ||
								(!forward && nc == Operators[i][0] && c == Operators[i][1]))
						{
							if (forward)
								token += nc;
							else
								token.prepend(nc);
							Offset += inc;
							break;
						}
					}
					return token;
				}
				break;
			case identifier:
				if (!toIsIdent(nc))
					return token;
				break;
			case string:
				if (c == endString)
				{
					if (nc == endString)
					{
						if (forward)
						{
							token += nc;
							Offset++;
						}
						else
						{
							token.prepend(nc);
							Offset--;
						}
					}
					else
						return token;
				}
				break;
			}
		}
	}
	return token;
}

QString toSQLParse::stringTokenizer::remaining(bool eol)
{
	QString ret;
	if (eol)
	{
//		 int pos = String.find('\n', Offset);
		int pos = String.indexOf('\n', Offset);
		if (pos < 0)
			pos = Offset;
		ret = String.mid(Offset, pos - Offset);
		Offset = pos;
	}
	else
	{
		ret = String.mid(Offset);
		Offset = String.length();
	}
	return ret;
}

toSQLParse::editorTokenizer::editorTokenizer(SqlEditorWidget *editor, int offset, int line)
		: tokenizer(offset, line)
{
	Editor = editor;
//	 toHighlightedText *text = dynamic_cast<toHighlightedText *>(editor);
//	 if (text)
//		 setAnalyzer(text->analyzer());
}

QString toSQLParse::editorTokenizer::getToken(bool forward, bool comments)
{
	bool first = true;
	while (Line < int(Editor->lines()) && Line >= 0)
	{
		QString line = Editor->text(Line);
		if (!first)
		{
			if (forward)
				Offset = 0;
			else
				Offset = line.length();
		}
		stringTokenizer token(line, /*SQLITEMAN analyzer(),*/ Offset, forward);
		QString ret = token.getToken(forward, true);
		Offset = token.offset();

		if (!ret.isNull())
		{
			if (forward)
			{
				QString end;
				if (ret.startsWith(("/*")) &&
						(ret.at(ret.length() - 2) != '*' ||
						 ret.at(ret.length() - 1) != '/'))
				{
					end = ("*/");
				}
				else if (ret.startsWith("'") && ((ret.count("'") % 2) != 0 || ret.at(ret.length() - 1) != '\''))
				{
					end = ("'");
				}
				else if (ret.startsWith(QUOTE_CHARACTER /*SQLITEMAN analyzer().quoteCharacter()*/) &&
						 ((ret.count(QUOTE_CHARACTER /*SQLITEMAN analyzer().quoteCharacter()*/) % 2) != 0 || ret.at(ret.length() - 1) != QUOTE_CHARACTER/*SQLITEMAN analyzer().quoteCharacter()*/))
				{
					end = QUOTE_CHARACTER; // SQLITEMAN analyzer().quoteCharacter();
				}
				if (!end.isNull())
				{
					for (Line++;
							Line < int(Editor->lines()) && (Offset = Editor->text(Line).indexOf(end)) < 0;
							Line++)
						ret += ("\n") + Editor->text(Line);
					if (Line < int(Editor->lines()))
					{
						ret += ("\n") + Editor->text(Line).mid(0, Offset + end.length());
						Offset += end.length();
					}
				}
			}
			else
			{
				QString end;
				if (ret.length() >= 2 &&
						ret.at(ret.length() - 2) == '*' &&
						ret.at(ret.length() - 1) == '/' &&
						!ret.startsWith(("/*")))
				{
					end = ("/*");
				}
				else if ((ret.length() >= 1 && ret.at(ret.length() - 1) == '\'') &&
						 (ret.length() < 2 || ret[0] != '\''))
				{
					end = ("\'");
				}
				else if ((ret.length() >= 1 && ret.at(ret.length() - 1) == QUOTE_CHARACTER /*SQLITEMAN analyzer().quoteCharacter()*/) &&
						 (ret.length() < 2 || ret.at(0) != QUOTE_CHARACTER /*SQLITEMAN analyzer().quoteCharacter()*/))
				{
					end = QUOTE_CHARACTER; // SQLITEMAN analyzer().quoteCharacter();
				}
				if (!end.isNull())
				{
					for (Line--;
							Line >= 0 && (Offset = Editor->text(Line).lastIndexOf/*findRev*/(end)) < 0;
							Line--)
						ret.prepend(Editor->text(Line) + ("\n"));
					if (Line >= 0)
					{
						QString str = Editor->text(Line);
						ret.prepend(str.mid(Offset, str.length() - Offset) + ("\n"));
					}
				}
			}
			if (comments || (!ret.startsWith(("/*")) && !ret.startsWith(("--")) && !ret.startsWith("//")))
				return ret;
			else
			{
				first = true;
				continue;
			}
		}
		Line += (forward ? 1 : -1);
		first = false;
	}
	return QString::null;
}

QString toSQLParse::editorTokenizer::remaining(bool eol)
{
	if (Line >= Editor->lines())
		return QString::null;
	if (!eol)
	{
		QStringList rows;
		rows << Editor->text(Line).mid(Offset);
		for (int i = Line;i < Editor->lines();i++)
			rows << Editor->text(i);
		Line = Editor->lines();
		Offset = 0;
		return rows.join(("\n"));
	}
	else
	{
		QString line = Editor->text(Line);
		QString ret = line.mid(offset());
		Offset = line.length();
		return ret;
	}
}

/*#define TOPARSE_DEBUG
#include <QtDebug>*/
toSQLParse::statement toSQLParse::parseStatement(tokenizer &tokens, bool declare, bool lst)
{
	statement ret(statement::Statement);

//	 toSyntaxAnalyzer &syntax = tokens.analyzer();

	QString first;
	QString realfirst;
	bool nokey = false;
	bool block = false;
	for (QString token = tokens.getToken(true, true);
			!token.isNull();
			token = tokens.getToken(true, true))
	{
		QString upp = token.toUpper();

		if (first.isNull() && !token.startsWith(("/*")) && !token.startsWith("--") && !token.startsWith("//"))
			realfirst = first = upp;

#ifdef TOPARSE_DEBUG
        printf("%s (%d)\n", (const char*)token.toUtf8(), tokens.line());
        printf("    %s - %s\n", (const char*)first.toUtf8(), (const char*)realfirst.toUtf8());
#endif

// SQLITEMAN
		 if (upp == ("PROCEDURE") ||
				 upp == ("FUNCTION") ||
				 upp == ("PACKAGE"))
         {
//              qDebug() << "PROCEDURE";
			 block = true;
         }

		 if (upp == ("SELF"))
         {
//              qDebug() << "SELF";
			 block = false;
         }

        if (upp == "BEGIN" && (first.isNull() || first == "BEGIN"))
        {
//             qDebug() << "plain BEGIN";
            ret.subTokens().insert(ret.subTokens().end(), statement(statement::Keyword, token, tokens.line()));
            nokey = false;            
        }
		else if (first != ("END") && ((first == ("IF") && upp == ("THEN")) ||
								  upp == ("LOOP") ||
								  upp == ("DO") ||
								  (/*syntax.declareBlock()*/true && upp == ("DECLARE")) ||
								  (block && upp == ("AS")) ||
								  (block && upp == ("IS")) ||
								  ((!declare || block) && upp == ("BEGIN"))))
		 {
//              qDebug() << "first != (\"END\") ";
			 block = false;
			 statement blk(statement::Block);
			 ret.subTokens().insert(ret.subTokens().end(), statement(statement::Keyword, token, tokens.line()));
			 blk.subTokens().insert(blk.subTokens().end(), ret);
			 statement cur(statement::Statement);
			 bool dcl = (upp == ("DECLARE") || upp == ("IS") || upp == ("AS"));
			 do
			 {
				 cur = parseStatement(tokens, dcl, false);
				 if (cur.Type == statement::List)
				 {
					 QMessageBox::warning(QApplication::activeWindow(), "Sqliteman",
										  "toSQLparse: Unbalanced parenthesis (Too many ')')");
				 }
				 blk.subTokens().insert(blk.subTokens().end(), cur);
				 if (cur.subTokens().begin() != cur.subTokens().end() &&
						 (*(cur.subTokens().begin())).String.toUpper() == ("BEGIN"))
					 dcl = false;
			 }
			 while (cur.subTokens().begin() != cur.subTokens().end() &&
					 (*cur.subTokens().begin()).String.toUpper() != ("END"));
			 return blk;
		 }
		 else if (((first == "IF" && upp == "THEN") ||
				   (first == "WHEN" && upp == "THEN") ||
				   (first == "ELSIF" && upp == "THEN") ||
				   upp == ("BEGIN") ||
				   upp == ("EXCEPTION") ||
				   first == ("ELSE")) && !lst)
		 {
//              qDebug() << "else if first==IF";
			 ret.subTokens().insert(ret.subTokens().end(), statement(statement::Keyword, token, tokens.line()));
			 return ret;
		 }
		 else if (first == ("ASSIGN") ||
				  first == ("SET") ||
				  first == ("PROMPT") ||
				  first == ("COLUMN") ||
				  first == ("SPOOL") ||
				  first == ("STORE") ||
				  first == ("REMARK") ||
				  first == ("REM"))
		 {
//              qDebug() << "ASSIGN";
			 ret.subTokens().insert(ret.subTokens().end(), statement(statement::Keyword, token, tokens.line()));
			 int line = tokens.line();
			 int offset = tokens.offset();
			 for (QString tmp = tokens.getToken(true, true);line == tokens.line();tmp = tokens.getToken(true, true))
				 ret.subTokens().insert(ret.subTokens().end(), statement(statement::Token, tmp, line));
			 tokens.setLine(line);
			 tokens.setOffset(offset);
			 tokens.remaining(true);
			 return ret;
		 }
		 else if (upp == (",") ||
// 		if (upp == (",") ||
//				  (syntax.reservedWord(upp) &&
				  (isKeyword(upp) &&
				  upp != ("NOT") &&
				  upp != ("IS") &&
				  upp != ("LIKE") &&
				  upp != ("IN") &&
				  upp != ("ELSE") &&
				  upp != ("ELSIF") &&
				  upp != ("END") &&
				  upp != ("BETWEEN") &&
				  upp != ("ASC") &&
				  upp != ("DESC") &&
				  upp != ("NULL")) && !nokey)
		{

		}
		else if (upp == ("("))
		{
//             qDebug() << "start (";
			ret.subTokens().insert(ret.subTokens().end(), statement(statement::Token, token, tokens.line()));
			statement lst = parseStatement(tokens, false, true);
			statement t = toPop(lst.subTokens());
			if (lst.Type != statement::List)
			{
				QMessageBox::warning(QApplication::activeWindow(), "Sqliteman",
									 "toSQLparse: Unbalanced parenthesis (Too many '(')");
			}
			nokey = false;
			if (first == ("CREATE") && !block)
			{
				statement end = parseStatement(tokens, false, true);
				statement blk(statement::Block);
				blk.subTokens().insert(blk.subTokens().end(), ret);
				blk.subTokens().insert(blk.subTokens().end(), lst);
				end.subTokens().insert(end.subTokens().begin(), t);
				blk.subTokens().insert(blk.subTokens().end(), end);
				return blk;
			}
			else
			{
				ret.subTokens().insert(ret.subTokens().end(), lst);
				ret.subTokens().insert(ret.subTokens().end(), t);
			}
		}
		else if (upp == (")"))
		{
//             qDebug() << "end )";
			ret.Type = statement::List;
			ret.subTokens().insert(ret.subTokens().end(), statement(statement::Token, token, tokens.line()));
			return ret;
		}
		else if (upp == (";"))
		{
//             qDebug() << "bodkociarka";
			ret.subTokens().insert(ret.subTokens().end(), statement(statement::Token, token, tokens.line()));
			return ret;
		}
		else if (upp.startsWith(("/*+")) || upp.startsWith(("--+")))
		{
//             qDebug() << "hint --+";
			QString com = token;
			if (com.startsWith(("--+")))
				com = ("/*+ ") + com.mid(3) + (" */");
			ret.subTokens().insert(ret.subTokens().end(), statement(statement::Token,
								   com.simplified(), tokens.line()));
		}
		else if (upp.startsWith(("/*")) || upp.startsWith(("--")) || upp.startsWith("//"))
		{
//             qDebug() << "comment";
			if ( ret.subTokens().empty() )
			{
				if (ret.Comment.isNull())
					ret.Comment = token;
				else
					ret.Comment += ("\n") + token;
			}
			else
			{
				QString &com = (*ret.subTokens().rbegin()).Comment;
				if (com.isEmpty())
					com = token;
				else
					com += ("\n") + token;
			}
		}
		else
		{
//             qDebug() << "plain else" <<token<< tokens.line();
			ret.subTokens().insert(ret.subTokens().end(), statement(statement::Token, token, tokens.line()));
			nokey = (token == ("."));
		}
		if (upp == ("AS") || upp == ("IS"))
        {
//             qDebug() << "setting first: " << upp;
			first = upp;
        }
		else if (first == ("IS") && upp == ("NULL"))
        {
//             qDebug() << "setting first (real): " << realfirst;
			first = realfirst;
        }
	}
	return ret;
}

std::list<toSQLParse::statement> toSQLParse::parse(tokenizer &tokens)
{
	std::list<toSQLParse::statement> ret;
	statement cur(statement::Statement);
	for (cur = parseStatement(tokens, false, false);
			cur.subTokens().begin() != cur.subTokens().end();
			cur = parseStatement(tokens, false, false))
	{
		if (cur.Type == statement::List)
		{
			QMessageBox::warning(QApplication::activeWindow(), "Sqliteman",
								 "toSQLparse: Unbalanced parenthesis (Too many ')')");
		}
		ret.insert(ret.end(), cur);
	}
	QString str = tokens.remaining(false);
	if (!str.isEmpty())
		ret.insert(ret.end(), statement(statement::Raw,
										str, tokens.line()));
	return ret;
}

toSQLParse::statement toSQLParse::parseStatement(tokenizer &tokens)
{
	statement cur(statement::Statement);
	cur = parseStatement(tokens, false, false);
	if (cur.Type == statement::List)
	{
		QMessageBox::warning(QApplication::activeWindow(), "Sqliteman",
							 "toSQLparse: Unbalanced parenthesis (Too many ')')");
	}
	return cur;
}

int toSQLParse::countIndent(const QString &txt, int &chars)
{
	int level = 0;
	while (txt[chars].isSpace() && chars < int(txt.length()))
	{
//		 char c = txt[chars].latin1();
		QChar c(txt[chars]);
		if (c == '\n')
			level = 0;
		else if (c == ' ')
			level++;
		else if (c == '\t')
			level = (level / DEF_TAB_WIDTH /*SQLITEMAN toMarkedText::defaultTabWidth()*/ + 1) * DEF_TAB_WIDTH/*toMarkedText::defaultTabWidth()*/;
		chars++;
	}
	return level;
}

toSQLParse::settings toSQLParse::Settings = {true,
		false,
		false,
		false,
		true,
		true,
		true,
		4,
		60
											};

QString toSQLParse::indentString(int level)
{
	QString ret;
	if (Settings.ExpandSpaces)
	{
		for (int i = 0;i < level / 8;i++)
			ret += ("\t");
		for (int j = 0;j < level % 8;j++)
			ret += (" ");
	}
	else
		for (int j = 0;j < level;j++)
			ret += (" ");
	return ret;
}

static int CurrentColumn(const QString &txt)
{
	int pos = txt.lastIndexOf(("\n"));
	if (pos < 0)
		pos = 0;
	else
		pos++;

	int level = 0;
	while (pos < int(txt.length()))
	{
//		 char c = txt[pos].latin1();
		QChar c(txt[pos]);
		if (c == '\n')
			level = 0;
		else if (c == '\t')
			level = (level / DEF_TAB_WIDTH /*SQLITEMAN toMarkedText::defaultTabWidth()*/ + 1) * DEF_TAB_WIDTH /*toMarkedText::defaultTabWidth()*/;
		else
			level++;
		pos++;
	}
	return level;

}

static QString IndentComment(int level, int current, const QString &comment, bool endNl)
{
	bool nl = true;
	QString ret;
	if (comment.length())
	{
		if (level <= current && (level || current))
		{
			ret += ("\n");
			current = 0;
		}
		for (int i = 0;i < comment.length();i++)
		{
			if (!nl || !comment.at(i).isSpace())
			{
				if (nl)
				{
					if (current == 0)
						ret += toSQLParse::indentString(level);
					else
					{
						while (current < level)
						{
							ret += (" ");
							current++;
						}
					}
					if (comment.at(i) == '*')
					{
						ret += (" ");
						current++;
					}
					nl = false;
				}
				ret += comment.at(i);
				if (comment.at(i) == '\n')
				{
					current = 0;
					nl = true;
				}
				else
					nl = false;
			}
		}
		if (!nl)
			ret += ("\n");
	}
	else if (endNl)
	{
		ret = ("\n");
	}
	return ret;
}

static QString AddComment(const QString &old, const QString &comment)
{
	QString ret = old;
	if (!ret.isEmpty() && !comment.isEmpty())
		ret += ("\n");
	ret += comment;
	return ret;
}

// QString toSQLParse::indentStatement(statement &stat, int level)
// {
//	 return indentStatement(stat, level, toSyntaxAnalyzer::defaultAnalyzer());
// }
// 
// QString toSQLParse::indentStatement(statement &stat, toConnection &conn, int level)
// {
//	 return indentStatement(stat, level, conn.analyzer());
// }

QString toSQLParse::indentStatement(statement &stat, int level/*SQLITEMAN, toSyntaxAnalyzer &syntax*/)
{
	QString ret;

	switch (stat.Type)
	{
	default:
		{
			QMessageBox::warning(QApplication::activeWindow(), "Sqliteman",
								 "toSQLparse: Internal error in toSQLParse, should never get here");
		}
	case statement::Block:
	{
		ret = IndentComment(level, 0, stat.Comment, false);
		int exc = 0;
		for (std::list<toSQLParse::statement>::iterator i = stat.subTokens().begin();
				i != stat.subTokens().end();
				i++)
		{
			int add
			= 0;
			std::list<toSQLParse::statement>::iterator j = i;
			j++;
			if (i != stat.subTokens().begin() &&
					j != stat.subTokens().end())
				add
				= Settings.IndentLevel;
			else
				exc = 0;

			QString t;
			if ((*i).subTokens().begin() != (*i).subTokens().
					end())
				t = (*(*i).subTokens().begin()).String.toUpper();
			if (t == ("BEGIN") || t == ("WHEN") || t == ("ELSE") || t == ("ELSIF"))
				add
				= 0;
			if ((*i).Type == statement::List)
				ret += indentString(level + add
									+ exc);
			ret += indentStatement(*i, level + add
								   + exc/*SQLITEMAN , syntax*/);
			if ((*i).Type == statement::List)
			{
				int i;
				for (i = ret.length() - 1;i >= 0 && ret[i].isSpace();i--)
					;
				ret = ret.mid(0, std::max(i + 1, 0));
				ret += ("\n");
				ret += indentString(level + exc);
			}
			if (t == ("EXCEPTION"))
				exc = Settings.IndentLevel * 2;
		}
		if (Settings.EndBlockNewline && level != 0)
			ret += ("\n");
	}
	break;
	case statement::List:
	case statement::Statement:
		int maxlev = 0;
		int maxlevorig = 0;
		bool useMaxLev = false;
		bool any = true;
		int current;
		bool first;
		bool noKeyBreak = false;
		bool lineList = false;
		QString comment;
		if (stat.Type == statement::Statement)
		{
			ret = IndentComment(level, 0, stat.Comment, false);
			useMaxLev = true;
			first = true;
			current = 0;
		}
		else
		{
			for (std::list<toSQLParse::statement>::iterator i = stat.subTokens().begin();
					i != stat.subTokens().end();)
			{
				if ((*i).Type != statement::Keyword)
					noKeyBreak = true;
				else
					useMaxLev = true;
				break;
			}
			current = level;
			first = true;
		}
		if (useMaxLev)
		{
			int count = 0;
			for (std::list<toSQLParse::statement>::iterator i = stat.subTokens().begin();
					i != stat.subTokens().end();
					i++)
			{
				if (any)
				{
					QString upp = (*i).String.toUpper();
					if ((*i).Type == statement::Keyword &&
							upp != ("LOOP") &&
							upp != ("DO") &&
							upp != ("THEN") &&
							upp != ("AS") &&
							upp != ("IS"))
					{
						if (int((*i).String.length()) + 1 > maxlev)
							maxlev = (*i).String.length() + 1;
						count++;
						any = false;
					}
					else if (i == stat.subTokens().begin())
					{
						noKeyBreak = true;
						break;
					}
				}
				else if ((*i).Type == statement::Token)
					any = true;
				if ((*i).Type == statement::List)
					count++;
			}
			if (count <= 1 && maxlev > 0)
				maxlev--;
			maxlevorig = maxlev;
			any = true;
		}

		for (std::list<toSQLParse::statement>::iterator i = stat.subTokens().begin();
				i != stat.subTokens().end();
				i++)
		{
			comment = AddComment(comment, (*i).Comment);
			QString upp = (*i).String.toUpper();

#ifdef TOPARSE_DEBUG
			printf("%s\n", (const char*)(*i).String.toUtf8());
#endif

			if ((*i).Type == statement::List)
			{
				if (Settings.OperatorSpace)
				{
					ret += (" ");
					current++;
				}
				QString t = indentStatement(*i, current/*SQLITEMAN, syntax*/);
				if (t.indexOf(("\n")) >= 0)
					current = CurrentColumn(t);
				else
					current += CurrentColumn(t);
				ret += t;
				any = true;
			}
			else if ((*i).String == (","))
			{
				if (Settings.CommaBefore)
				{
					ret += IndentComment(Settings.CommentColumn, current, comment, true);
					comment = QString::null;
					ret += indentString(level + maxlev - (Settings.OperatorSpace ? 2 : 1));
					ret += (",");
				}
				else
				{
					ret += (",");
					ret += IndentComment(Settings.CommentColumn, current + 1, comment, true);
					comment = QString::null;
					ret += indentString(level + maxlev);
				}
				current = level + maxlev;
				any = false;
				lineList = true;
			}
			else if ((*i).Type == statement::Keyword && (upp == ("LOOP") ||
					 upp == ("DO") ||
					 upp == ("THEN") ||
					 upp == ("AS") ||
					 upp == ("IS")))
			{
				if (!Settings.BlockOpenLine)
				{
					if (ret.length() > 0)
					{
						if (toIsIdent(ret.at(ret.length() - 1)) ||
								ret.at(ret.length() - 1) == QUOTE_CHARACTER /*SQLITEMAN syntax.quoteCharacter()*/ ||
								ret.at(ret.length() - 1) == '\'' ||
								Settings.OperatorSpace)
						{
							ret += (" ");
							current++;
						}
					}
					ret += Settings.KeywordUpper ? (*i).String.toUpper() : (*i).String;
					current += (*i).String.length();
				}
				else
				{
					ret += IndentComment(Settings.CommentColumn, current, comment, true);
					comment = QString::null;
					ret += indentString(level);
					ret += Settings.KeywordUpper ? (*i).String.toUpper() : (*i).String;
					current = level + (*i).String.length();
				}
				any = false;
			}
			else if (any && (*i).Type == statement::Keyword && !noKeyBreak)
			{
				if (first)
					first = false;
				else
				{
					ret += IndentComment(Settings.CommentColumn, current, comment, true);
					current = 0;
					comment = QString::null;
				}
				if (current == 0)
				{
					ret += indentString(level);
					current = level;
				}
				else
					while (current < level)
					{
						ret += (" ");
						current++;
					}
				maxlev = maxlevorig;
				QString word = Settings.KeywordUpper ? (*i).String.toUpper() : (*i).String;
				if (ret.length())
				{
					ret += QString("%1").arg(word,
											 Settings.RightSeparator ? maxlev - 1 : 1 - maxlev);
					current = level + std::max(int(word.length()), maxlev - 1);
				}
				else
				{
					ret += word;
					current = level + word.length();
				}
				any = false;
				lineList = false;
			}
			else
			{
				QString t = (*i).String;
				bool add
				= false;
				if ((*i).Type == statement::Keyword)
				{
					if (!lineList &&
							!any &&
							(*i).Type == statement::Keyword &&
							!noKeyBreak &&
							upp == ("BY"))
						add
						= true;
				}
				else
				{
					any = true;
				}
				if (isKeyword(upp) /*SQLITEMAN syntax.reservedWord(upp)*/ && Settings.KeywordUpper)
					t = upp;

				int extra;
				if (first)
				{
					first = false;
					any = false;
					extra = 0;
				}
				else
				{
					if (ret.length() > 0 &&
							!ret.at(ret.length() - 1).isSpace() &&
							(Settings.OperatorSpace || ((toIsIdent(t[0]) ||
														 t[0] == QUOTE_CHARACTER /*SQLITEMAN syntax.quoteCharacter()*/ || t[0] == '\'') &&
														(toIsIdent(ret.at(ret.length() - 1)) ||
														 ret.at(ret.length() - 1) == QUOTE_CHARACTER /*SQLITEMAN syntax.quoteCharacter()*/ ||
														 ret.at(ret.length() - 1) == '\'')
													   )
							)
					   )
					{
						if (t != (";") &&
								t != (".") &&
								ret.at(ret.length() - 1) != '.' &&
								current != 0)
						{
							current++;
							ret += (" ");
						}
					}
					else if (ret.length() > 2 && ret.at(ret.length() - 2) == '*' && ret.at(ret.length() - 1) == '/')
					{
						current++;
						ret += (" ");
					}
					extra = maxlev;
				}
				if (current < level + maxlev)
				{
					if (current == 0)
						ret += indentString(level + maxlev);
					else
						while (current < level + maxlev)
						{
							ret += (" ");
							current++;
						}
					current = level + maxlev;
				}
				ret += t;
				current += t.length();
				if (t.startsWith(("<<")))
				{
					ret += ("\n");
					current = 0;
				}

				if (add
				   )
					maxlev += t.length() + 1;
			}
		}
		if (stat.Type == statement::Statement)
		{
			ret += IndentComment(Settings.CommentColumn, current, comment, true);
			comment = QString::null;
			if (Settings.EndBlockNewline &&
					level == 0 &&
					stat.subTokens().begin() != stat.subTokens().end() &&
					(*stat.subTokens().rbegin()).String == (";"))
				ret += ("\n");
		}
		else if (!comment.isEmpty())
		{
			ret += IndentComment(Settings.CommentColumn, current, comment, true);
			comment = QString::null;
			ret += indentString(level - (Settings.OperatorSpace ? 2 : 1));
		}
		break;
	}
	return ret;
}

// QString toSQLParse::indent(const QString &str)
// {
//	 return indent(str, toSyntaxAnalyzer::defaultAnalyzer());
// }
// 
// QString toSQLParse::indent(const QString &str, toConnection &conn)
// {
//	 return indent(str, conn.analyzer());
// }
/*
QString toSQLParse::indent(std::list<statement> &stat)
{
	return indent(stat, toSyntaxAnalyzer::defaultAnalyzer());
}*/
/*
QString toSQLParse::indent(std::list<statement> &stat, toConnection &conn)
{
	return indent(stat, conn.analyzer());
}*/

QString toSQLParse::indent(const QString &str/*SQLITEMAN, toSyntaxAnalyzer &syntax*/)
{
	stringTokenizer tokenizer(str/*SQLITEMAN, syntax*/);
	std::list<toSQLParse::statement> blk = parse(tokenizer);
	int pos = 0;
	int level = countIndent(str, pos);

	QString ret;
	for (std::list<toSQLParse::statement>::iterator i = blk.begin();
			i != blk.end();
			i++)
	{
		ret += indentStatement(*i, level/*SQLITEMAN, syntax*/);
	}
	pos = ret.length();
	while (pos > 0 && ret[pos - 1].isSpace())
	{
		pos--;
	}
	return ret.mid(0, pos) + ("\n");
}

QString toSQLParse::indent(std::list<statement> &stat/*SQLITEMAN, toSyntaxAnalyzer &syntax*/)
{
	int pos = 0;

	QString ret;
	for (std::list<toSQLParse::statement>::iterator i = stat.begin();
			i != stat.end();
			i++)
	{
		ret += indentStatement(*i, 0/*SQLITEMAN, syntax*/);
	}
	pos = ret.length();
	while (pos > 0 && ret[pos - 1].isSpace())
	{
		pos--;
	}
	return ret.mid(0, pos) + ("\n");
}

// std::list<toSQLParse::statement> toSQLParse::parse(const QString &str/*, toConnection &conn*/)
// {
//	 stringTokenizer tokenizer(str/*, conn.analyzer()*/);
//	 return parse(tokenizer);
// }
/*
toSQLParse::statement toSQLParse::parseStatement(toSQLParse::tokenizer &tokens, toConnection &conn)
{
	tokens.setAnalyzer(conn.analyzer());
	return parseStatement(tokens);
}*/

// toSyntaxAnalyzer &toSQLParse::tokenizer::analyzer()
// {
//	 if (Analyzer)
//		 return *Analyzer;
//	 else
//		 return toSyntaxAnalyzer::defaultAnalyzer();
// }
