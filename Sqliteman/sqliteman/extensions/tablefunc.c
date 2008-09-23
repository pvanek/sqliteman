/*
The functions was coded by Alexey Pechnikov (pechnikov@mobigroup.ru) and tested on linux only.
The code is public domain. 

Compile as
     gcc -fPIC -lm -shared tablefunc.c -o libsqlitetablefunc.so
*/

#if !defined(SQLITE_CORE) || defined(SQLITE_ENABLE_TABLEFUNC)

#include <stdlib.h>
#include <sys/types.h>
#include <string.h>
#include <stdio.h>

#include <assert.h>

#ifndef SQLITE_CORE
  #include "sqlite3ext.h"
  SQLITE_EXTENSION_INIT1
#else
  #include "sqlite3.h"
#endif

/*
function may work with 64-bit integers

from, to, step, table name
create table testrange(rowid);
select intrange2table (1,10,1,'testrange');
select * from testrange;
1
2
3
4
5
6
7
8
9
10

select intrange2table (10000000000,100000000000,10000000000,'testrange');
select * from testrange;
1
2
3
4
5
6
7
8
9
10
10000000000
20000000000
30000000000
40000000000
50000000000
60000000000
70000000000
80000000000
90000000000
100000000000
*/
static void intrange2table4Func(
	sqlite3_context *context,
	int argc,
	sqlite3_value **argv
) {
	u_int64_t i;
	const unsigned char *zTable;
	sqlite3 *db;
	sqlite3_stmt *pStmt;        /* A statement */
	int rc;                     /* Result code */
	char *zSql;                 /* An SQL statement */

	if( sqlite3_value_type(argv[0]) == SQLITE_NULL || sqlite3_value_type(argv[1]) == SQLITE_NULL  || \
sqlite3_value_type(argv[2]) == SQLITE_NULL  || sqlite3_value_type(argv[3]) == SQLITE_NULL ){
		sqlite3_result_null(context);
		return;
	}
	zTable = sqlite3_value_text(argv[3]);
	db = (sqlite3*) sqlite3_context_db_handle(context);
	zSql = sqlite3_mprintf("INSERT INTO %Q (rowid) VALUES (?)", zTable);
   	rc = sqlite3_prepare(db, zSql, -1, &pStmt, 0);
	sqlite3_free(zSql);
   	if( rc != SQLITE_OK ){
		sqlite3_result_error(context, sqlite3_errmsg(db), -1);
		return;
   	}
	for (i=sqlite3_value_int64(argv[0]);i<=sqlite3_value_int64(argv[1]);i=i+sqlite3_value_int64(argv[2])) {
		sqlite3_bind_int64(pStmt, 1, i);
		sqlite3_step(pStmt);
		if( rc != SQLITE_OK ) {
			sqlite3_result_error(context, sqlite3_errmsg(db), -1);
			return;
		}
		rc = sqlite3_reset(pStmt);
	}
	sqlite3_finalize(pStmt);
	return;
}

/*
create table testrange(rowid);
select intrange2table (78312604812,78312604814,'testrange');
select * from testrange;
78312604812
78312604813
78312604814
*/
static void intrange2table3Func(
	sqlite3_context *context,
	int argc,
	sqlite3_value **argv
) {
	u_int64_t i;
	const unsigned char *zTable;
	sqlite3 *db;
	sqlite3_stmt *pStmt;        /* A statement */
	int rc;                     /* Result code */
	char *zSql;                 /* An SQL statement */

	if( sqlite3_value_type(argv[0]) == SQLITE_NULL || sqlite3_value_type(argv[1]) == SQLITE_NULL  || \
sqlite3_value_type(argv[2]) == SQLITE_NULL ){
		sqlite3_result_null(context);
		return;
	}
	zTable = sqlite3_value_text(argv[2]);
	db = (sqlite3*) sqlite3_context_db_handle(context);
	zSql = sqlite3_mprintf("INSERT INTO %Q (rowid) VALUES (?)", zTable);
   	rc = sqlite3_prepare(db, zSql, -1, &pStmt, 0);
	sqlite3_free(zSql);
   	if( rc != SQLITE_OK ){
		sqlite3_result_error(context, sqlite3_errmsg(db), -1);
		return;
   	}
	for (i=sqlite3_value_int64(argv[0]);i<=sqlite3_value_int64(argv[1]);i++) {
		sqlite3_bind_int64(pStmt, 1, i);
		sqlite3_step(pStmt);
		if( rc != SQLITE_OK ) {
			sqlite3_result_error(context, sqlite3_errmsg(db), -1);
			return;
		}
		rc = sqlite3_reset(pStmt);
	}
	sqlite3_finalize(pStmt);
	return;
}

/*
create table testrange(rowid);
select intargs2table('testrange',1,2,3,4,5);
select * from testrange;
1
2
3
4
5

create table testrange2(field);
select intargs2table('testrange2','field',1,2,3,4,5);
select * from testrange2;
1
2
3
4
5
*/
static void intargs2tableFunc(
	sqlite3_context *context,
	int argc,
	sqlite3_value **argv
) {
	u_int64_t i;
	const unsigned char *zTable, *zField;
	sqlite3 *db;
	sqlite3_stmt *pStmt;        /* A statement */
	int rc;                     /* Result code */
	char *zSql;                 /* An SQL statement */

	if( sqlite3_value_type(argv[0]) == SQLITE_NULL || sqlite3_value_type(argv[1]) == SQLITE_NULL || argc <= 2 ){
		sqlite3_result_null(context);
		return;
	}
	zTable = sqlite3_value_text(argv[0]);
	zField = sqlite3_value_text(argv[1]);
	db = (sqlite3*) sqlite3_context_db_handle(context);
	zSql = sqlite3_mprintf("INSERT INTO %Q (%q) VALUES (?)", zTable, zField);
   	rc = sqlite3_prepare(db, zSql, -1, &pStmt, 0);
	sqlite3_free(zSql);
   	if( rc != SQLITE_OK ){
		sqlite3_result_error(context, sqlite3_errmsg(db), -1);
		return;
   	}
	for (i=2;i<argc;i++) {
		sqlite3_bind_int64(pStmt, 1, sqlite3_value_int64(argv[i]));
		sqlite3_step(pStmt);
		if( rc != SQLITE_OK ) {
			sqlite3_result_error(context, sqlite3_errmsg(db), -1);
			return;
		}
		rc = sqlite3_reset(pStmt);
	}
	sqlite3_finalize(pStmt);
	return;
}


/*
create table testrange(field);
select args2table('testrange','field','1','2','3','4','5','00');
select * from testrange;
1
2
3
4
5
00

create table testrange(field);
select args2table('testrange','field',6,7,8,9,00);
select * from testrange;
6
7
8
9
0

Attention: 00 is typed as integer 0 in function arguments but inserted as text '0'

*/
static void args2tableFunc(
	sqlite3_context *context,
	int argc,
	sqlite3_value **argv
) {
	u_int64_t i;
	const unsigned char *zTable, *zField;
	sqlite3 *db;
	sqlite3_stmt *pStmt;        /* A statement */
	int rc;                     /* Result code */
	char *zSql;                 /* An SQL statement */

	if( sqlite3_value_type(argv[0]) == SQLITE_NULL || sqlite3_value_type(argv[1]) == SQLITE_NULL || argc <= 2 ){
		sqlite3_result_null(context);
		return;
	}
	zTable = sqlite3_value_text(argv[0]);
	zField = sqlite3_value_text(argv[1]);
	db = (sqlite3*) sqlite3_context_db_handle(context);
	zSql = sqlite3_mprintf("INSERT INTO %Q (%q) VALUES (?)", zTable, zField);
   	rc = sqlite3_prepare(db, zSql, -1, &pStmt, 0);
	sqlite3_free(zSql);
   	if( rc != SQLITE_OK ){
		sqlite3_result_error(context, sqlite3_errmsg(db), -1);
		return;
   	}
	for (i=2;i<argc;i++) {
		sqlite3_bind_text(pStmt, 1, sqlite3_value_text(argv[i]), -1, SQLITE_STATIC);
		sqlite3_step(pStmt);
		if( rc != SQLITE_OK ) {
			sqlite3_result_error(context, sqlite3_errmsg(db), -1);
			return;
		}
		rc = sqlite3_reset(pStmt);
	}
	sqlite3_finalize(pStmt);
	return;
}

/* SQLite invokes this routine once when it loads the extension.
** Create new functions, collating sequences, and virtual table
** modules here.  This is usually the only exported symbol in
** the shared library.
*/

int sqlite3TablefuncInit(sqlite3 *db){
  static const struct {
     char *zName;
     signed char nArg;
     int argType;           /* 1: 0, 2: 1, 3: 2,...  N:  N-1. */
     int eTextRep;          /* 1: UTF-16.  0: UTF-8 */
     void (*xFunc)(sqlite3_context*,int,sqlite3_value **);
  } aFuncs[] = {
	{ "intrange2table",      4, 0, SQLITE_UTF8,    intrange2table4Func },
	{ "intrange2table",      3, 0, SQLITE_UTF8,    intrange2table3Func },
	{ "intargs2table",      -1, 0, SQLITE_UTF8,    intargs2tableFunc },
	{ "args2table",         -1, 0, SQLITE_UTF8,    args2tableFunc },
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
  return sqlite3TablefuncInit(db);
}
#endif

#endif
