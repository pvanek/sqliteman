/*
The functions was coded by Alexey Pechnikov (pechnikov@mobigroup.ru) and tested on linux only.
The code is public domain. 

Compile as
     gcc -fPIC -lm -luuid -shared uuid.c -o libsqliteuuid.so
*/

#if !defined(SQLITE_CORE) || defined(SQLITE_ENABLE_UUID)

#include <stdlib.h>
#include <sys/types.h>
#include <string.h>
#include <stdio.h>
#include <uuid/uuid.h>

#include <assert.h>

#ifndef SQLITE_CORE
  #include "sqlite3ext.h"
  SQLITE_EXTENSION_INIT1
#else
  #include "sqlite3.h"
#endif

/*
UUID generate
*/
static void uuidGenerateFunc(
	sqlite3_context *context,
	int argc,
	sqlite3_value **argv
) {
	uuid_t u;
	char uuid[37];
	uuid_generate(u);
	uuid_unparse(u, uuid);
	sqlite3_result_text( context, (char*)uuid, -1, SQLITE_TRANSIENT);
}

/* SQLite invokes this routine once when it loads the extension.
** Create new functions, collating sequences, and virtual table
** modules here.  This is usually the only exported symbol in
** the shared library.
*/

int sqlite3UuidInit(sqlite3 *db){
  static const struct {
     char *zName;
     signed char nArg;
     int argType;           /* 1: 0, 2: 1, 3: 2,...  N:  N-1. */
     int eTextRep;          /* 1: UTF-16.  0: UTF-8 */
     void (*xFunc)(sqlite3_context*,int,sqlite3_value **);
  } aFuncs[] = {
	{ "uuid_generate",      0, 0, SQLITE_UTF8,    uuidGenerateFunc }
  };

  int i;
  for(i=0; i<sizeof(aFuncs)/sizeof(aFuncs[0]); i++){
    void *pArg;
    int argType = aFuncs[i].argType;
    pArg = (void*)(int)argType;
    sqlite3_create_function(db, aFuncs[i].zName, aFuncs[i].nArg,
        aFuncs[i].eTextRep, pArg, aFuncs[i].xFunc, 0, 0);
  }

  return 0;
}

#if !SQLITE_CORE
int sqlite3_extension_init(
  sqlite3 *db, 
  char **pzErrMsg,
  const sqlite3_api_routines *pApi
){
  SQLITE_EXTENSION_INIT2(pApi)
  return sqlite3UuidInit(db);
}
#endif

#endif
