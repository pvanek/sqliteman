/*
For general Sqliteman copyright and licensing information please refer
to the COPYING file provided with the program. Following this notice may exist
a copyright and/or license notice that predates the release of Sqliteman
for which a new license (GPL+exception) is in place.

This file is only a header-wrapper for required functions from shell.c.
*/

#ifndef SHELL_H
#define SHELL_H

#ifdef __cplusplus
extern "C" {
#endif

#include "sqlite3.h"


/*
** These are the allowed modes.
*/
#define MODE_Line     0  /* One column per line.  Blank line between records */
#define MODE_Column   1  /* One record per line in neat columns */
#define MODE_List     2  /* One record per line with a separator */
#define MODE_Semi     3  /* Same as MODE_List but append ";" to each line */
#define MODE_Html     4  /* Generate an XHTML table */
#define MODE_Insert   5  /* Generate SQL "insert" statements */
#define MODE_Tcl      6  /* Generate ANSI-C or TCL quoted elements */
#define MODE_Csv      7  /* Quote strings, numbers are plain */
#define MODE_Explain  8  /* Like MODE_Column, but do not truncate data */

struct previous_mode_data {
	int valid;        /* Is there legit data in here? */
	int mode;
	int showHeader;
	int colWidth[100];
};

/*
** An pointer to an instance of this structure is passed from
** the main program to the callback.  This is used to communicate
** state and mode information.
*/
struct callback_data {
	sqlite3 *db;            /* The database */
	int echoOn;            /* True to echo input commands */
	int cnt;               /* Number of records displayed so far */
	FILE *out;             /* Write results here */
	int mode;              /* An output mode setting */
	int writableSchema;    /* True if PRAGMA writable_schema=ON */
	int showHeader;        /* True to show column names in List or Column mode */
	char *zDestTable;      /* Name of destination table when MODE_Insert */
	char separator[20];    /* Separator character for MODE_List */
	int colWidth[100];     /* Requested width of each column when in column mode*/
	int actualWidth[100];  /* Actual width of each column */
	char nullvalue[20];    /* The text to print when a NULL comes back from
	** the database */
	struct previous_mode_data explainPrev;
	/* Holds the mode information just before
	** .explain ON */
	char outfile[FILENAME_MAX]; /* Filename for *out */
	const char *zDbFilename;    /* name of the database file */
};

/*
** Execute a query statement that has a single result column.  Print
** that result column on a line by itself with a semicolon terminator.
**
** This is used, for example, to show the schema of the database by
** querying the SQLITE_MASTER table.
*/
/*static */int run_table_dump_query(FILE *out, sqlite3 *db, const char *zSelect);

/*
** Run zQuery.  Use dump_callback() as the callback routine so that
** the contents of the query are output as SQL statements.
**
** If we get a SQLITE_CORRUPT error, rerun the query after appending
** "ORDER BY rowid DESC" to the end.
*/
/*static */int run_schema_dump_query(
  struct callback_data *p, 
  const char *zQuery,
  char **pzErrMsg
);

#ifdef __cplusplus
}  /* End of the 'extern "C"' block */
#endif

#endif
