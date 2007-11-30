/*
For general Sqliteman copyright and licensing information please refer
to the COPYING file provided with the program. Following this notice may exist
a copyright and/or license notice that predates the release of Sqliteman
for which a new license (GPL+exception) is in place.
*/
#include <QStringList>

#include "sqlkeywords.h"


QStringList sqlKeywords()
{
	QStringList ret;
	ret << "select" << "from" << "where"
		<< "or" << "and" << "join"
		<< "outer" << "left" << "right"
		<< "all" << "distinct" << "group by"
		<< "having" << "order by" << "limit"
		<< "offset" << "as" << "natural"
		<< "left" << "right" << "full"
		<< "outer" << "inner" << "cross"
		<< "on" << "using" << "collate"
		<< "asc" << "desc" << "union"
		<< "union all" << "intersect" << "except"
		<< "explain"
		// alter table
		<< "alter" << "table" << "rename"
		<< "to" << "add" << "column"
		// analyze
		<< "analyze" << "vacuum"
		// attach/detach
		<< "attach" << "database" << "detach"
		// transaction
		<< "begin" << "deferred" << "immediate"
		<< "exclusive" << "transaction" << "end"
		<< "commit" << "rollback"
		// create ix
		<< "create" << "unique" << "index"
		<< "if" << "not" << "exists"
		// drop foo / insert
		<< "drop" << "into" << "values" << "replace"
		<< "set"
		// create table
		<< "temp" << "temporary" << "table" << "constraint"
		<< "null" << "primary" << "key" << "autoincrement"
		<< "unique" << "check" << "default" << "collate"
		<< "conflict" << "virtual"
		// create trigger
		<< "trigger" << "before" << "after"
		<< "instead of" << "delete" << "insert"
		<< "update" << "for" << "each" << "row"
		<< "statement"
		<< "abort" << "fail" << "ignore" << "replace"
		// create view
		<< "view"
		// expressions
		<< "escape" << "isnull" << "notnull"
		<< "between" << "case" << "then"
		<< "else" << "cast" << "like"
		<< "glob" << "regexp" << "match"
		<< "pragma" << "reindex";

	return ret;
}

bool isKeyword(const QString & w)
{
	return sqlKeywords().contains(w, Qt::CaseInsensitive);
}
