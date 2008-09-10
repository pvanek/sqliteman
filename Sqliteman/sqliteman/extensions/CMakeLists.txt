MESSAGE(STATUS "Extension dir")

INCLUDE_DIRECTORIES( ${SQLITE_INCLUDE_DIR} )

ADD_LIBRARY(sqlitecompress SHARED compress.c)
ADD_LIBRARY(sqlitefunctions SHARED functions.c)
ADD_LIBRARY(sqliteipv4 SHARED ipv4-ext.c)
