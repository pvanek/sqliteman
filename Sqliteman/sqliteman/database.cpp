/*
For general Sqliteman copyright and licensing information please refer
to the COPYING file provided with the program. Following this notice may exist
a copyright and/or license notice that predates the release of Sqliteman
for which a new license (GPL+exception) is in place.
*/

#include <QSqlDriver>
#include <QSqlQuery>
#include <QSqlError>
#include <QTextStream>
#include <QVariant>
#include <QFile>
#include <QMessageBox>

#include "database.h"
#include "preferences.h"
#include "shell.h"


void Database::exception(const QString & message)
{
	QMessageBox::critical(0, tr("SQL Error"), message);
}

bool Database::execSql(QString statement)
{
	QSqlQuery query(statement, QSqlDatabase::database(SESSION_NAME));
	if(query.lastError().isValid())
	{
		exception(tr("Error executing: %1.").arg(query.lastError().text()));
		return false;
	}
	return true;
}

QString Database::sessionName(const QString & schema)
{
	return QString("%1_%2").arg(SESSION_NAME).arg(schema);
}

DbAttach Database::getDatabases()
{
	DbAttach ret;
	QSqlQuery query("PRAGMA database_list;", QSqlDatabase::database(SESSION_NAME));

	if (query.lastError().isValid())
	{
		exception(tr("Cannot get databases list. %1").arg(query.lastError().text()));
		return ret;
	}
	while(query.next())
		ret.insertMulti(query.value(1).toString(), query.value(2).toString());
	return ret;
}

bool Database::dropTable(const QString & table, const QString & schema)
{
	QString sql = QString("DROP TABLE \"%1\".\"%2\";").arg(schema).arg(table);
	QSqlQuery query(sql, QSqlDatabase::database(SESSION_NAME));
	
	if(query.lastError().isValid())
	{
		exception(tr("Error while dropping table %1: %2.").arg(table).arg(query.lastError().text()));
		return false;
	}
	return true;
}

FieldList Database::tableFields(const QString & table, const QString & schema)
{
	FieldList fields;
	QString sql(QString("PRAGMA \"%1\".TABLE_INFO(\"%2\");").arg(schema).arg(table));
	QSqlQuery query(sql, QSqlDatabase::database(SESSION_NAME));
	if (query.lastError().isValid())
	{
		exception(tr("Error while getting the fileds of %1: %2.").arg(table).arg(query.lastError().text()));
		return fields;
	}

	// Build a query string to SELECT the CREATE statement from sqlite_master
	QString createSQL(QString("SELECT sql FROM sqlite_master WHERE name=\"%1\";").arg(table)); 
	// Run the query
	QSqlQuery createQuery(createSQL, QSqlDatabase::database(SESSION_NAME));
	// Make sure the query ran successfully
	if(createQuery.lastError().isValid()) {
		exception(tr("Error grabbing CREATE statement: %1: %2.").arg(table).arg(createQuery.lastError().text()));
	}
	// Position the Query on the first (only) result
	createQuery.first();
	// Grab the complete CREATE statement
	QString createStatement = createQuery.value(0).toString();
	// Reduce the CREATE statement down to just the field info
	createStatement.replace(QRegExp("CREATE TABLE .* \\((.*)\\).*"), "\\1");
	// Make a list with all of the individual field statements
	QStringList params = createStatement.split(QRegExp(","));
	// Initialize ourselfs a Field Map -- keys and vals are QStrings
	QHash<QString, QString> fieldMap;
	// Hashify the params list
	while(!params.isEmpty())
	{
		// e.g. "id INTEGER PRIMARY KEY"
		QString parameter = params.takeFirst().trimmed();
		// Tokenize the parameter
		QStringList words = parameter.split(" ");
		// Grab the field name
		QString fieldName = words.takeFirst().remove('"');
		// Grab the full field type
		QString fieldType = words.join(" ");
		// Populate the hash
		fieldMap[fieldName] = fieldType;
	}

	while (query.next())
	{
		DatabaseTableField field;
		field.cid = query.value(0).toInt();
		field.name = query.value(1).toString();
		field.type = query.value(2).toString();
		if (field.type.isNull() || field.type.isEmpty())
			field.type = "NULL";
		field.notnull = query.value(3).toBool();
		field.defval = query.value(4).toString();
		field.pk = query.value(5).toBool();
		if (field.pk) {
			field.type += " PRIMARY KEY";
			// autoincrement keyword?
			// adapted from http://stackoverflow.com/questions/16724409/how-to-programmatically-determine-whether-a-column-is-set-to-autoincrement-in-sq
			QString autoincSql(QString("SELECT 1 FROM %1.sqlite_master WHERE lower(name) = \"%2\" AND sql LIKE '%\"%3\" %4 AUTOINCREMENT%';").arg(schema).arg(table).arg(field.name).arg(field.type));
			QSqlQuery autoincQuery(autoincSql, QSqlDatabase::database(SESSION_NAME));
			if (!autoincQuery.lastError().isValid() && autoincQuery.first())
				field.type += " AUTOINCREMENT";
		}
		field.comment = "";
		fields.append(field);
	}

	return fields;
}

QStringList Database::indexFields(const QString & index, const QString &schema)
{
	QString sql(QString("PRAGMA \"%1\".INDEX_INFO(\"%2\");").arg(schema).arg(index));
	QSqlQuery query(sql, QSqlDatabase::database(SESSION_NAME));
	QStringList fields;

	if (query.lastError().isValid())
	{
		exception(tr("Error while getting the fileds of %1: %2.").arg(index).arg(query.lastError().text()));
		return fields;
	}

	while (query.next())
		fields.append(query.value(2).toString());

	return fields;
}

DbObjects Database::getObjects(const QString type, const QString schema)
{
	DbObjects objs;

	QString sql;
	if (type.isNull())
		sql = QString("SELECT lower(name), lower(tbl_name) FROM \"%1\".sqlite_master;").arg(schema);
	else
		sql = QString("SELECT lower(name), lower(tbl_name) FROM \"%1\".sqlite_master WHERE type = '%2' and name not like 'sqlite_%';").arg(schema).arg(type);

	QSqlQuery query(sql, QSqlDatabase::database(SESSION_NAME));
	while(query.next())
		objs.insertMulti(query.value(1).toString(), query.value(0).toString());

	if(query.lastError().isValid())
		exception(tr("Error while the list of %1: %2.").arg(type).arg(query.lastError().text()));

	return objs;
}

QStringList Database::getSysIndexes(const QString & table, const QString & schema)
{
	QStringList orig = Database::getObjects("index", schema).values(table);
	// really all indexes
	QStringList sysIx;
	QSqlQuery query(QString("PRAGMA \"%1\".index_list(\"%2\");").arg(schema).arg(table),
					QSqlDatabase::database(SESSION_NAME));

	QString curr;
	while(query.next())
	{
		curr = query.value(1).toString();
		if (!orig.contains(curr))
			sysIx.append(curr);
	}

	if(query.lastError().isValid())
		exception(tr("Error while the list of the system catalogue: %2.").arg(query.lastError().text()));

	return sysIx;
}

DbObjects Database::getSysObjects(const QString & schema)
{
	DbObjects objs;

	QSqlQuery query(QString("SELECT lower(name), lower(tbl_name) FROM \"%1\".sqlite_master WHERE type = 'table' and name like 'sqlite_%';").arg(schema),
					QSqlDatabase::database(SESSION_NAME));

	objs.insert("sqlite_master", "");
	while(query.next())
		objs.insertMulti(query.value(1).toString(), query.value(0).toString());

	if(query.lastError().isValid())
		exception(tr("Error while the list of the system catalogue: %2.").arg(query.lastError().text()));

	return objs;
}

bool Database::dropView(const QString & view, const QString & schema)
{
	QString sql = QString("DROP VIEW \"%1\".\"%2\";").arg(schema).arg(view);
	QSqlQuery query(sql, QSqlDatabase::database(SESSION_NAME));
	
	if(query.lastError().isValid())
	{
		exception(tr("Error while dropping the view %1: %2.").arg(view).arg(query.lastError().text()));
		return false;
	}
	return true;
}

bool Database::dropIndex(const QString & name, const QString & schema)
{
	QString sql = QString("DROP INDEX \"%1\".\"%2\"").arg(schema).arg(name);
	QSqlQuery query(sql, QSqlDatabase::database(SESSION_NAME));
	
	if(query.lastError().isValid())
	{
		exception(tr("Error while dropping the index %1: %2.").arg(name).arg(query.lastError().text()));
		return false;
	}
	return true;
}

bool Database::exportSql(const QString & fileName)
{
	QFile file(fileName);
	
	if(!file.open(QIODevice::WriteOnly | QIODevice::Text))
	{
		exception(tr("Unable to open file %1 for writing.").arg(fileName));
		return false;
	}

	QTextStream stream(&file);
	
	// Run query for tables
	QString sql = "SELECT sql FROM sqlite_master;";
	QSqlQuery query(sql, QSqlDatabase::database(SESSION_NAME));
	
	if (query.lastError().isValid())
	{
		exception(tr("Error while exporting SQL: %1.").arg(query.lastError().text()));
		return false;
	}
	
	while(query.next())
		stream << query.value(0).toString() << ";\n";
	
	file.close();
	return true;
}

// TODO/FIXME: it definitelly requires worker thread - to unfreeze GUI
bool Database::dumpDatabase(const QString & fileName)
{
	char * fname = fileName.toUtf8().data();
	FILE * f = fopen(fname, "w");
	
	if (f == NULL)
	{
		exception(tr("Unable to open file %1 for writing.").arg(fileName));
		return false;
	}

	struct callback_data p;

	sqlite3 * h = Database::sqlite3handle();
	Q_ASSERT_X(h!=0, "Database::dumpDatabase", "sqlite3handle is missing");
	p.db = h;
	p.mode = MODE_Insert;
	p.out = f;
	p.writableSchema = 0;
	sqlite3_exec(p.db, "PRAGMA writable_schema=ON", 0, 0, 0);
	fprintf(p.out, "BEGIN TRANSACTION;\n");
	run_schema_dump_query(&p,
        "SELECT name, type, sql FROM sqlite_master "
        "WHERE sql NOT NULL AND type=='table'", 0
	);
	run_table_dump_query(p.out, p.db,
        "SELECT sql FROM sqlite_master "
        "WHERE sql NOT NULL AND type IN ('index','trigger','view')"
	);
	fprintf(p.out, "COMMIT;\n");
	sqlite3_exec(p.db, "PRAGMA writable_schema=OFF", 0, 0, 0);

	fclose(f);
	return true;
}

QString Database::describeObject(const QString & name,
								 const QString & schema)
{
	QString sql("select sql from \"%1\".sqlite_master where lower(name) = \"%2\";");
	QSqlQuery query(sql.arg(schema).arg(name), QSqlDatabase::database(SESSION_NAME));
	
	if (query.lastError().isValid())
	{
		exception(tr("Error while describe object %1: %2.").arg(name).arg(query.lastError().text()));
		return "";
	}
	
	while(query.next())
		return query.value(0).toString();
	
	return "";
}

bool Database::dropTrigger(const QString & name, const QString & schema)
{
	QString sql = QString("DROP TRIGGER \"%1\".\"%2\";").arg(schema).arg(name);
	QSqlQuery query(sql, QSqlDatabase::database(SESSION_NAME));
	
	if(query.lastError().isValid())
	{
		exception(tr("Error while dropping the trigger %1: %2.").arg(name).arg(query.lastError().text()));
		return false;
	}
	return true;
}

QString Database::hex(const QByteArray & val)
{
	const char hexdigits[] = {
		'0', '1', '2', '3', '4', '5', '6', '7',
		'8', '9', 'A', 'B', 'C', 'D', 'E', 'F'
	};
	QString ret("X'");
	for (int i = 0; i < val.size(); ++i)
	{
		ret += hexdigits[(val.at(i)>>4)&0xf];
		ret += hexdigits[val.at(i)&0xf];
	}
	return ret + "'";
}

QString Database::pragma(const QString & name)
{
	QString statement("PRAGMA main.%1;");
	QSqlQuery query(statement.arg(name), QSqlDatabase::database(SESSION_NAME));
	if (query.lastError().isValid())
	{
		exception(tr("Error executing: %1.").arg(query.lastError().text()));
		return "error";
	}

	while(query.next())
		return query.value(0).toString();
	return tr("Not Set");
}

sqlite3 * Database::sqlite3handle()
{
	QVariant v = QSqlDatabase::database(SESSION_NAME).driver()->handle();
	if (!v.isValid())
	{
		exception(tr("DB driver is not valid"));
		return 0;
	}
	if (qstrcmp(v.typeName(), "sqlite3*") != 0)
	{
		exception(tr("DB type name does not equal sqlite3"));
		return 0;
	}

	sqlite3 *handle = *static_cast<sqlite3 **>(v.data());
	if (handle == 0)
		exception(tr("DB handler is not valid"));

	return handle;
}

bool Database::setEnableExtensions(bool enable)
{
	sqlite3 * handle = Database::sqlite3handle();
	if (handle && sqlite3_enable_load_extension(handle, enable ? 1 : 0) != SQLITE_OK)
	{
		if (enable)
			exception(tr("Failed to enable extension loading"));
		else
			exception(tr("Failed to disable extension loading"));
		return false;
	}
	return true;
}

QStringList Database::loadExtension(const QStringList & list)
{
	sqlite3 * handle = Database::sqlite3handle();
	if (!handle)
		return QStringList();

	QStringList retval;

	foreach(QString f, list)
	{
		const char *zProc = 0;
		char *zErrMsg = 0;
		int rc;

		rc = sqlite3_load_extension(handle, f.toUtf8().data(), zProc, &zErrMsg);
		if (rc != SQLITE_OK)
		{
			exception(tr("Error loading exception\n%1\n%2").arg(f).arg(QString::fromLocal8Bit(zErrMsg)));
			sqlite3_free(zErrMsg);
		}
		else
			retval.append(f);
	}
	return retval;
}
