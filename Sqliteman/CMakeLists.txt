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
#Set the custom CMake module directory where our include/lib finders are
SET(CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake/modules")

#PROJECT SETUP
PROJECT(sqliteman)
SET (VERSION_MAJOR "1")
SET (VERSION_MINOR "1")
SET (VERSION_PATCH "0")
SET (VERSION_SUFFIX "svn")
SET (VERSION ${VERSION_MAJOR}.${VERSION_MINOR}.${VERSION_PATCH}${VERSION_SUFFIX})

SET (EXE_NAME "sqliteman")

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


IF (WANT_INTERNAL_QSCINTILLA)
    MESSAGE(STATUS "QSci: Building with internal copy of QScintilla.")
    MESSAGE(STATUS "QSci: It's recomended to use external library for it")
ELSE (WANT_INTERNAL_QSCINTILLA)

    FIND_PACKAGE(QScintilla REQUIRED)
    IF (QSCINTILLA_FOUND)
        MESSAGE("QScintilla2 Found OK")
        ADD_DEFINITIONS("-DQT -DSCI_LEXER")
    ELSE (QSCINTILLA_FOUND)
        MESSAGE(STATUS "QSci: No QScintilla2 libraries found")
        MESSAGE(STATUS "QSci: You can use -DWANT_INTERNAL_QSCINTILLA=1 to build included copy of it.")
        MESSAGE(FATAL_ERROR "No QScintilla2 found")
    ENDIF (QSCINTILLA_FOUND)

ENDIF (WANT_INTERNAL_QSCINTILLA)


ADD_SUBDIRECTORY( sqliteman )

IF (WIN32)
    MESSAGE(STATUS "Installation directories 'share' etc. aren't created in WIN32")
ELSE (WIN32)
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
