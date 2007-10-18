#
# This is the base config/build file for Sqliteman
# for more info about available options read: README
#

CMAKE_MINIMUM_REQUIRED( VERSION 2.4.3 )
#Pretty colors
SET(CMAKE_COLOR_MAKEFILE ON)
#Dont force verbose
SET(CMAKE_VERBOSE_MAKEFILE OFF)
#Include current dir
# SET(CMAKE_INCLUDE_CURRENT_DIR TRUE)

#PROJECT SETUP
PROJECT(sqliteman)
SET (VERSION_MAJOR "1")
SET (VERSION_MINOR "1")
SET (VERSION_PATCH "0")
SET (VERSION_SUFFIX "svn")
SET (VERSION ${VERSION_MAJOR}.${VERSION_MINOR}.${VERSION_PATCH}${VERSION_SUFFIX})

MESSAGE(STATUS "Sqliteman ${VERSION} will be built to install into ${CMAKE_INSTALL_PREFIX}")

# set platform dependent stuff - e.g. sqlite3 binary name
# NOTE: it's built with RESOURCES on Windows 
IF (WIN32)
    SET (SQLITE_BIN "sqlite3.exe")
    SET (WANT_RESOURCES 1)
ELSE (WIN32)
    SET (SQLITE_BIN "sqlite3")
ENDIF (WIN32)

# set paths to install
IF (WANT_RESOURCES)
    SET (TRANSLATION_DIR ":/ts/")
    SET (DOC_DIR ":/doc/")
    SET (ICON_DIR ":/icons/")
ELSE (WANT_RESOURCES)
    SET (TRANSLATION_DIR "${CMAKE_INSTALL_PREFIX}/share/sqliteman/")
    SET (DOC_DIR "${CMAKE_INSTALL_PREFIX}/share/sqliteman/doc/")
    SET (ICON_DIR "${CMAKE_INSTALL_PREFIX}/share/sqliteman/icons")
ENDIF (WANT_RESOURCES)


# defines for compiler based on the previous setup
ADD_DEFINITIONS("-DTRANSLATION_DIR=\\\"${TRANSLATION_DIR}\\\"")
ADD_DEFINITIONS("-DDOC_DIR=\\\"${DOC_DIR}\\\"")
ADD_DEFINITIONS("-DICON_DIR=\\\"${ICON_DIR}\\\"")
# defines for compiler
ADD_DEFINITIONS("-DSQLITE_BINARY=\\\"${SQLITE_BIN}\\\"")
ADD_DEFINITIONS("-DSQLITEMAN_VERSION=\\\"${VERSION}\\\"")
IF (WANT_RESOURCES)
    ADD_DEFINITIONS("-DWANT_RESOURCES")
ENDIF (WANT_RESOURCES)


#uninstall
CONFIGURE_FILE(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake_uninstall.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/cmake_uninstall.cmake"
    IMMEDIATE @ONLY)

ADD_CUSTOM_TARGET(uninstall
    "${CMAKE_COMMAND}" -P "${CMAKE_CURRENT_BINARY_DIR}/cmake_uninstall.cmake")


IF (WANT_DEBUG)
    SET (CMAKE_BUILD_TYPE Debug)
    MESSAGE (STATUS " *** Debug build ***")
ELSE(WANT_DEBUG)
    SET(CMAKE_BUILD_TYPE RELEASE)
    MESSAGE(STATUS " *** Release build (use -DWANT_DEBUG=1 for debug one) ***")
ENDIF(WANT_DEBUG)


SET(CMAKE_CXX_FLAGS_DEBUG "-O0 -g3 -Wall")
SET(CMAKE_C_FLAGS_DEBUG "-O0 -g3 -Wall")
SET(CMAKE_CXX_FLAGS_RELEASE "-O2 -Wall")
SET(CMAKE_C_FLAGS_RELEASE "-O2 -Wall")


SET (QT_MT_REQUIRED true)
SET (QT_MIN_VERSION "4.3.0")
SET( QT_USE_QTSQL TRUE )
SET( QT_USE_QTXML TRUE )
FIND_PACKAGE( Qt4 REQUIRED )

INCLUDE( ${QT_USE_FILE} )

ADD_SUBDIRECTORY( sqliteman )

IF (not WIN32)
    # install desktop
    INSTALL(FILES
        sqliteman.desktop
        DESTINATION "share/applications"
    )
    INSTALL(FILES
        sqliteman/icons/sqliteman.png
        DESTINATION "share/icons"
    )
ENDIF (WIN32)
