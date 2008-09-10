/*
gcc -lz -lm -fPIC -shared compress.c -o libsqlitecompress.so
*/

#if !defined(SQLITE_CORE) || defined(SQLITE_ENABLE_COMPRESS)

#include <stdlib.h>
#include <sys/types.h>
#include <string.h>
#include "zlib.h"

#include <assert.h>

#ifndef SQLITE_CORE
  #include "sqlite3ext.h"
  SQLITE_EXTENSION_INIT1
#else
  #include "sqlite3.h"
#endif

// see http://www.mail-archive.com/sqlite-users%40sqlite.org/msg17018.html
/*
** SQL function to compress content into a blob using libz
*/
static void compressFunc(
  sqlite3_context *context,
  int argc,
  sqlite3_value **argv
){
  int nIn, nOut;
  long int nOut2;
  const unsigned char *inBuf;
  unsigned char *outBuf;
  assert( argc==1 );
  nIn = sqlite3_value_bytes(argv[0]);
  inBuf = sqlite3_value_blob(argv[0]);
  nOut = 13 + nIn + (nIn+999)/1000;
  outBuf = malloc( nOut+4 );
  outBuf[0] = nIn>>24 & 0xff;
  outBuf[1] = nIn>>16 & 0xff;
  outBuf[2] = nIn>>8 & 0xff;
  outBuf[3] = nIn & 0xff;
  nOut2 = (long int)nOut;
  compress(&outBuf[4], &nOut2, inBuf, nIn);
  sqlite3_result_blob(context, outBuf, nOut2+4, free);
}

/*
** An SQL function to decompress.
*/
static void uncompressFunc(
  sqlite3_context *context,
  int argc,
  sqlite3_value **argv
){
  unsigned int nIn, nOut, rc;
  const unsigned char *inBuf;
  unsigned char *outBuf;
  long int nOut2;

  assert( argc==1 );
  nIn = sqlite3_value_bytes(argv[0]);
  if( nIn<=4 ){
    return;
  }
  inBuf = sqlite3_value_blob(argv[0]);
  nOut = (inBuf[0]<<24) + (inBuf[1]<<16) + (inBuf[2]<<8) + inBuf[3];
  outBuf = malloc( nOut );
  nOut2 = (long int)nOut;
  rc = uncompress(outBuf, &nOut2, &inBuf[4], nIn);
  if( rc!=Z_OK ){
    free(outBuf);
  }else{
    sqlite3_result_blob(context, outBuf, nOut2, free);
  }
}


/* SQLite invokes this routine once when it loads the extension.
** Create new functions, collating sequences, and virtual table
** modules here.  This is usually the only exported symbol in
** the shared library.
*/

int sqlite3CompressInit(sqlite3 *db){
  static const struct {
     char *zName;
     signed char nArg;
     int argType;           /* 1: 0, 2: 1, 3: 2,...  N:  N-1. */
     int eTextRep;          /* 1: UTF-16.  0: UTF-8 */
     void (*xFunc)(sqlite3_context*,int,sqlite3_value **);
  } aFuncs[] = {
    { "compress",           1, 0, SQLITE_UTF8,    compressFunc },
    { "uncompress",         1, 0, SQLITE_UTF8,    uncompressFunc },
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
  return sqlite3CompressInit(db);
}
#endif

#endif
