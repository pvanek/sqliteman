
INCLUDE_DIRECTORIES(${CMAKE_SOURCE_DIR}/sqliteman/sqlite) #${SQLITE_INCLUDE_DIR} )


IF (UNIX)
    SET (EXTENSION_INSTALL lib/sqliteman)
ENDIF (UNIX)

IF (WIN32)
    SET (EXTENSION_INSTALL extensions)
ENDIF (WIN32)



# sample dummy extension on all platforms
SET (EXT_DUMMY "sqlitedummy")
ADD_LIBRARY(${EXT_DUMMY} MODULE dummy.c)
INSTALL(TARGETS ${EXT_DUMMY} LIBRARY DESTINATION ${EXTENSION_INSTALL})

IF (UNIX)

IF (APPLE)
        MESSAGE(STATUS "")
        MESSAGE(STATUS "Extension SqliteCompress cannot be bult on MacOSX, volunteers to port welcomed.")
        MESSAGE(STATUS "")
ELSE (APPLE)
	SET(EXT_COMPRESS "sqlitecompress")
	ADD_LIBRARY(${EXT_COMPRESS} MODULE compress.c)
	INSTALL(TARGETS ${EXT_COMPRESS} LIBRARY DESTINATION ${EXTENSION_INSTALL})
ENDIF (APPLE)

	SET(EXT_FUNCTIONS "sqlitefunctions")
	ADD_LIBRARY(${EXT_FUNCTIONS} MODULE functions.c)
	INSTALL(TARGETS ${EXT_FUNCTIONS} LIBRARY DESTINATION ${EXTENSION_INSTALL})

	SET(EXT_IPV4 "sqliteipv4")
	ADD_LIBRARY(${EXT_IPV4} MODULE ipv4-ext.c)
	INSTALL(TARGETS ${EXT_IPV4} LIBRARY DESTINATION ${EXTENSION_INSTALL})

	SET(EXT_MD5 "sqlitemd5")
	ADD_LIBRARY(${EXT_MD5} MODULE md5.c)
	INSTALL(TARGETS ${EXT_MD5} LIBRARY DESTINATION ${EXTENSION_INSTALL})

	SET(EXT_TABLEPOPULATION "sqlitetablepopulation")
	ADD_LIBRARY(${EXT_TABLEPOPULATION} MODULE tablefunc.c)
	INSTALL(TARGETS ${EXT_TABLEPOPULATION} LIBRARY DESTINATION ${EXTENSION_INSTALL})

	# libuuid is mandatory! libuuid-devel on Suse
	FIND_PATH(UUID_INCLUDE uuid.h
		PATHS
			${UUID_INC_PATH}
			/usr/include/
			/usr/include/uuid/
	)
	FIND_LIBRARY(UUID_LIBRARY
		NAMES uuid libuuid
		PATHS ${UUID_LIB_PATH} /usr/lib/
	)
	if (UUID_LIBRARY AND UUID_INCLUDE)
		SET(EXT_UUID "sqliteuuid")
		ADD_LIBRARY(${EXT_UUID} MODULE uuid.c)
		INSTALL(TARGETS ${EXT_UUID} LIBRARY DESTINATION ${EXTENSION_INSTALL})
	else (UUID_LIBRARY AND UUID_INCLUDE)
		MESSAGE(" ")
		MESSAGE(STATUS "UUID library is not found. SqliteUUID extension won't be built.")
		MESSAGE(STATUS "You can specify its location with -DUUID_INC_PATH -DUUID_LIB_PATH")
		MESSAGE(STATUS "or install it in standard locations. Current status:")
		MESSAGE(STATUS "    includes: ${UUID_INCLUDE}")
		MESSAGE(STATUS "         lib: ${UUID_LIBRARY}")
		MESSAGE(" ")
	endif (UUID_LIBRARY AND UUID_INCLUDE)

IF (APPLE)
	MESSAGE(STATUS "")
	MESSAGE(STATUS "Extension VirtualText cannot be bult on MacOSX, volunteers to port welcomed.")
	MESSAGE(STATUS "")
ELSE (APPLE)
	SET(EXT_VIRTUALTEXT "sqlitevirtualtext")
	ADD_LIBRARY(${EXT_VIRTUALTEXT} MODULE virtualtext.c)
	INSTALL(TARGETS ${EXT_VIRTUALTEXT} LIBRARY DESTINATION ${EXTENSION_INSTALL})
ENDIF (APPLE)

	#####################################################################
	# subdirectory extensions ###########################################

	# libicu is mandatory! libicu-devel on sus
	FIND_PATH(ICU_INCLUDE ustring.h
		PATHS
			${ICU_INC_PATH}
			/usr/include/
			/usr/include/unicode/
	)
	FIND_LIBRARY(ICU_LIBRARY
		NAMES icudata
		PATHS ${ICU_LIB_PATH} /usr/lib/
	)
	if (ICU_INCLUDE AND ICU_LIBRARY)
		SET(EXT_ICU "sqliteicu")
		ADD_LIBRARY(${EXT_ICU} MODULE icu/icu.c)
		INSTALL(TARGETS ${EXT_ICU} LIBRARY DESTINATION ${EXTENSION_INSTALL})
	else (ICU_INCLUDE AND ICU_LIBRARY)
		MESSAGE(" ")
		MESSAGE(STATUS "ICU library is not found. SqliteICU extension won't be built.")
		MESSAGE(STATUS "You can specify its location with -DICU_INC_PATH -DICU_LIB_PATH")
		MESSAGE(STATUS "or install it in standard locations. Current status:")
		MESSAGE(STATUS "    includes: ${ICU_INCLUDE}")
		MESSAGE(STATUS "         lib: ${ICU_LIBRARY}")
		MESSAGE(" ")
	endif (ICU_INCLUDE AND ICU_LIBRARY)

	SET(EXT_RTREE "sqlitertree")
	ADD_LIBRARY(${EXT_RTREE} MODULE rtree/rtree.c)
	INSTALL(TARGETS ${EXT_RTREE} LIBRARY DESTINATION ${EXTENSION_INSTALL})

ENDIF (UNIX)

IF (WIN32)
    MESSAGE(STATUS) 
    MESSAGE(STATUS "Most of the sample sqlite extensions won't be built on MS Windows.")
    MESSAGE(STATUS "Volunteers to port it are welcomed")
    MESSAGE(STATUS)
ENDIF (WIN32)

