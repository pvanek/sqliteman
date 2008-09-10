#include <QtDebug>
#include <QSqlDatabase>
#include <QSqlDriver>
#include <QVariant>


int main (int argc, char ** argv)
{
	QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE", "testSession");
	db.setDatabaseName(":memory:");

    QVariant v = db.driver()->handle();
	qDebug() << "handle: " << v;

	qDebug() << "isValid: " << v.isValid();
	qDebug() << "typeName: " << v.typeName();

    if (v.isValid() && qstrcmp(v.typeName(), "sqlite3*")==0)
	{
		qDebug() << "true";
        // v.data() returns a pointer to the handle
//         sqlite3 *handle = *static_cast<sqlite3 **>(v.data());
//         if (handle != 0) { // check that it is not NULL
//             ...
//         }
    }

	return 0;
}
