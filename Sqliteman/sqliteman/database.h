/*
For general Sqliteman copyright and licensing information please refer
to the COPYING file provided with the program. Following this notice may exist
a copyright and/or license notice that predates the release of Sqliteman
for which a new license (GPL+exception) is in place.
*/

#ifndef DATABASE_H
#define DATABASE_H

#include <QCoreApplication>
#include <QSqlDatabase>
#include <QStringList>


#define SESSION_NAME "sqliteman-db"

/*! \brief This struct is a sqlite3 table column representation.
Something like a system catalogue item */
typedef struct
{
	int cid;
	QString name;
	QString type;
	bool notnull;
	QString defval;
	bool pk;
	QString comment;
}
DatabaseTableField;

/*! \brief List of the attached databases ("schemas").
Mapping name/filename */
typedef QMap<QString,QString> DbAttach;

//! \brief Table columns list.
typedef QList<DatabaseTableField> FieldList;

//! \brief A map with "object name"/"its parent" - schema
typedef QMap<QString,QString> DbObjects;


/*!
 * @brief The database manager
 * 
 * The %Database class represents a single database file/object, and provides convinient methods
 * for accessing specific elements in it (currently only tables and views). Though the application
 * still doesn't use this capability, it is completly safe to have multipile %Database object managing
 * diffirent databases in the same time.
 * 
 * Internally, the class uses the QtSQL API for manipulating the database.
 *
 * Almost all methods here are static so it's not needed to create a Database instance.
 *
 * \author Igor Khanin
 * \author Petr Vanek <petr@scribus.info>
 */
class Database
{
		Q_DECLARE_TR_FUNCTIONS(Database)
				
	public:
		static DbAttach getDatabases();
		/*! \brief Gather user objects from sqlite_master by type.
		It skips the resrved names "sqlite_%". See getSysObjects().
		\param type a "enum" 'table', 'view', index etc.
		\retval DbObjects a map with "object name"/"its parent"
		*/
		static DbObjects getObjects(const QString type, const QString schema = "main");

		/*! \brief Gather "SYS schema" objects.
		\param schema a string with "attached db" name
		\retval DbObjects with With reserved names "sqlite_%".
		*/
		static DbObjects getSysObjects(const QString & schema = "main");

		/*! \brief Gather "SYS indexes".
		System indexes are indexes created internally for UNIQUE constraints.
		\param table a table name.
		\param schema a string with "attached db" name
		\retval DbObjects with With reserved names "sqlite_%".
		*/
		static QStringList getSysIndexes(const QString & table, const QString & schema);

		/*! \brief Execute a SQL statement with no DB returns.
		\param statement a SQL command like INSERT/CREATE/DROP. Selects are not important here.
		\retval bool true on succes, false on error. Error are reported in this method
		             already. */
		static bool execSql(QString statement);

		/*! \brief Create a session name for the new DB connection.
		\param schema a schema name.
		\retval QString string with "SQLITEMAN_schema" format.
		*/
		static QString sessionName(const QString & schema);

		/*!
		@brief Drop a table from the database
		@param table The name of the table to drop
		\param schema a table own schema
		\retval bool true on success, false on error. Error are reported in this method
		             already. */
		static bool dropTable(const QString & table, const QString & schema);

		/*!
		@brief Drop a trigger from the database
		@param table The name of the trigger to drop
		\param schema a table own schema
		\retval bool true on success, false on error. Error are reported in this method
		             already. */
		static bool dropTrigger(const QString & name, const QString & schema);

		/*!
		* @brief Returns the list of fields in a table
		*
		* @param table The table to retrive the fields from
		* @return The list of fields in \a table
		*/
		static FieldList tableFields(const QString & table, const QString & schema);
		
		/*!
		@brief Drop a view from the database
		@param table The name of the view to drop
		\param schema a table own schema
		\retval bool true on success, false on error. Error are reported in this method
		             already. */
		static bool dropView(const QString & view, const QString & schema);

		/*!
		@brief Drop an index from the database
		@param table The name of the index to drop
		\param schema a table own schema
		\retval bool true on success, false on error. Error are reported in this method
		             already. */
		static bool dropIndex(const QString & name, const QString & schema);

		/*!
		@brief Exports the SQL code of a database to file
		If the file provided by \a fileName exists, it will be overriden.
		@param fileName The file to export the SQL to
		@todo Currently, Only the tables and views are exported. This should be fixed.
		*/
		static bool exportSql(const QString & fileName);

		/*! \brief BLOB X'foo' notation. See sqlite3 internals as a reference.
		\param val a raw "encoded" QByteArray (string)
		\retval QString with sqlite encoded X'blah' notation.
		*/
		static QString hex(const QByteArray & val);

	private:
		//! \brief Error feedback to the user.
		static void exception(const QString & message);
};

#endif
