# SPDX-FileCopyrightText: 2007-2026 Petr Vanek
# SPDX-License-Identifier: GPL-3.0-or-later

cmake_minimum_required(VERSION 3.25)

project(sqliteman
    VERSION 2.0.0
    DESCRIPTION "SQLite GUI database manager"
    LANGUAGES C CXX)

# The version above is the only one there is: the AppStream metainfo and the
# program itself take it from here. A release changes it and this date, the
# day it is released, for the metainfo.
set(SQLITEMAN_RELEASE_DATE 2026-10-25)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

option(SQLITEMAN_BUILD_TESTS "Build unit tests" ON)
option(SQLITEMAN_WERROR "Treat compiler warnings as errors" OFF)
option(SQLITEMAN_WITH_DOCS "Build the manual; needs Sphinx with the MyST parser" ON)
option(SQLITEMAN_SCREENSHOTS "Build the tool that takes the manual's screenshots" OFF)

set(SQLITEMAN_QT_COMPONENTS Concurrent Core Gui Widgets)
# The manual is shown in the program itself when Qt Help is there; without
# it the Help menu opens the website instead.
find_package(Qt6 6.5 QUIET COMPONENTS Help)
if(Qt6Help_FOUND)
    list(APPEND SQLITEMAN_QT_COMPONENTS Help)
endif()
if(SQLITEMAN_BUILD_TESTS OR SQLITEMAN_SCREENSHOTS)
    list(APPEND SQLITEMAN_QT_COMPONENTS Test)
endif()

find_package(Qt6 6.5 REQUIRED COMPONENTS ${SQLITEMAN_QT_COMPONENTS})
qt_standard_project_setup(REQUIRES 6.5)
# It gives installed programs a RUNPATH of $ORIGIN, for libraries installed
# beside them. There are none, as every library here is linked in
# statically, and distributions refuse such a RUNPATH in their packages.
set(CMAKE_INSTALL_RPATH "")

include(SqlitemanSQLite)
include(SqlitemanQScintilla)
include(SqlitemanCompilerOptions)

if(SQLITEMAN_BUILD_TESTS)
    enable_testing()
    include(SqlitemanTesting)
endif()

add_subdirectory(src/sqlite)
add_subdirectory(src/config)
add_subdirectory(src/sql)
add_subdirectory(src/catalog)
add_subdirectory(src/app)

if(SQLITEMAN_SCREENSHOTS)
    add_subdirectory(tools/screenshots)
endif()

if(SQLITEMAN_WITH_DOCS)
    add_subdirectory(docs)
endif()
