/*
A dummy extension for testing.
Sample usage: select helloWorld() from foo;

Petr Vanek <petr@scribus.info>
*/

#if !defined(SQLITE_CORE) || defined(SQLITE_ENABLE_COMPRESS)

#ifndef SQLITE_CORE
  #include "sqlite3ext.h"
  SQLITE_EXTENSION_INIT1
#else
  #include "sqlite3.h"
#endif


static void helloWorld(
  sqlite3_context *context,
  int argc,
  sqlite3_value **argv
){
    sqlite3_result_text(context, "Hello World", -1, SQLITE_STATIC);
}


/* SQLite invokes this routine once when it loads the extension.
** Create new functions, collating sequences, and virtual table
** modules here.  This is usually the only exported symbol in
** the shared library.
*/

int sqlite3helloWorldInit(sqlite3 *db){
  sqlite3_create_function(db, "helloWorld", -1, SQLITE_UTF8,  0, helloWorld, 0, 0);
  return 0;
}

#if !SQLITE_CORE
int sqlite3_extension_init(
  sqlite3 *db, 
  char **pzErrMsg,
  const sqlite3_api_routines *pApi
){
  SQLITE_EXTENSION_INIT2(pApi)
  return sqlite3helloWorldInit(db);
}
#endif

#endif 
