IF (UNIX)
INCLUDE_DIRECTORIES(${CMAKE_SOURCE_DIR}/sqliteman/sqlite) #${SQLITE_INCLUDE_DIR} )

SET(EXT_COMPRESS "sqlitecompress")
SET(EXT_FUNCTIONS "sqlitefunctions")
SET(EXT_IPV4 "sqliteipv4")
SET(EXT_MD5 "md5")
SET(EXT_TABLEPOPULATION "tablepopulation")

ADD_LIBRARY(${EXT_COMPRESS} MODULE compress.c)
ADD_LIBRARY(${EXT_FUNCTIONS} MODULE functions.c)
ADD_LIBRARY(${EXT_IPV4} MODULE ipv4-ext.c)
ADD_LIBRARY(${EXT_MD5} MODULE md5.c)
ADD_LIBRARY(${EXT_TABLEPOPULATION} MODULE tablefunc.c)


    INSTALL(TARGETS ${EXT_COMPRESS} ${EXT_FUNCTIONS} ${EXT_IPV4}
                    ${EXT_MD5} ${EXT_TABLEPOPULATION}
        LIBRARY DESTINATION lib/sqliteman)

ENDIF (UNIX)

IF (WIN32)
    MESSAGE(STATUS) 
    MESSAGE(STATUS "Sample sqlite extensions won't be built on MS Windows.")
    MESSAGE(STATUS "Volunteers to port it are welcomed")
    MESSAGE(STATUS)
ENDIF (WIN32)
