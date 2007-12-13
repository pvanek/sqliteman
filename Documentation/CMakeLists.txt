#
# This is the base config/build file for Sqliteman Documentation
#
# See README for more info.
#
# Howto add new language:
# create new lang directory, add ADD_SUBDIRECTORY(aLanguage),
# copy en/CMakeLists.txt to the new lang dir.
#

CMAKE_MINIMUM_REQUIRED( VERSION 2.4.3 )
#Pretty colors
SET(CMAKE_COLOR_MAKEFILE ON)
#Dont force verbose
SET(CMAKE_VERBOSE_MAKEFILE ON)
#Include current dir
# SET(CMAKE_INCLUDE_CURRENT_DIR TRUE)
#Set the custom CMake module directory where our include/lib finders are
# SET(CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake/modules")

#PROJECT SETUP
PROJECT(sqlitemandocs)
SET (VERSION_MAJOR "1")
SET (VERSION_MINOR "1")
SET (VERSION_PATCH "0")
SET (VERSION_SUFFIX "svn")
SET (VERSION ${VERSION_MAJOR}.${VERSION_MINOR}.${VERSION_PATCH}${VERSION_SUFFIX})

MESSAGE(STATUS "Sqliteman ${VERSION} Docs will be installed into ${CMAKE_INSTALL_PREFIX}")


FIND_PROGRAM(MEINPROC_EXECUTABLE NAMES meinproc4 meinproc)
IF (MEINPROC_EXECUTABLE)
    MESSAGE(STATUS "Meinproc4 found: ${MEINPROC_EXECUTABLE}")
    SET (xsltcmd "${MEINPROC_EXECUTABLE}")
    SET (xsltparams0 "--style")
    SET (xsltparams "${CMAKE_CURRENT_SOURCE_DIR}/kde-chunk.xsl")
ELSE (MEINPROC_EXECUTABLE)

    FIND_PROGRAM(XSLTPROC_EXECUTABLE xsltproc)
    IF (XSLTPROC_EXECUTABLE)
        MESSAGE(STATUS "Xsltproc found: ${XSLTPROC_EXECUTABLE}")
        SET (xsltcmd "${XSLTPROC_EXECUTABLE}")
        SET (xsltparams "${CMAKE_CURRENT_SOURCE_DIR}/kde-chunk.xsl")
    ELSE (XSLTPROC_EXECUTABLE)
         MESSAGE(FATAL_ERROR "No XSL processor found!")
    ENDIF (XSLTPROC_EXECUTABLE)

ENDIF (MEINPROC_EXECUTABLE)

MESSAGE(STATUS "Command to execute: ${xsltcmd} ${xsltparams0} ${xsltparams}")


ADD_SUBDIRECTORY (en)

