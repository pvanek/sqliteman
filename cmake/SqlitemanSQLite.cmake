# SPDX-FileCopyrightText: 2026 Petr Vanek
# SPDX-License-Identifier: GPL-3.0-or-later

# Provides the SQLite::SQLite3 target: the system's SQLite when it is recent
# enough and built with column metadata, otherwise the amalgamation
# downloaded and built as part of this project (the usual case on Windows and
# macOS).
#
# Column metadata (sqlite3_column_table_name() and friends) is what tells
# which table a result's rows come from, so that they can be edited.

option(SQLITEMAN_BUNDLED_SQLITE
    "Always download and build SQLite instead of using a system copy" OFF)

# ALTER TABLE ... DROP COLUMN, STRICT tables, the pragma table-valued
# functions this program reads the schema with, and sqlite3_error_offset()
# to point at a syntax error.
set(SQLITEMAN_SQLITE_MIN_VERSION 3.38)

if(NOT SQLITEMAN_BUNDLED_SQLITE)
    find_package(SQLite3 ${SQLITEMAN_SQLITE_MIN_VERSION})
    if(SQLite3_FOUND)
        include(CheckSourceCompiles)
        include(CMakePushCheckState)
        cmake_push_check_state(RESET)
        set(CMAKE_REQUIRED_LIBRARIES SQLite::SQLite3)
        check_source_compiles(C [=[
            #include <sqlite3.h>
            int main(void) { return sqlite3_column_table_name(0, 0) != 0; }
        ]=] SQLITEMAN_SQLITE_HAS_COLUMN_METADATA)
        cmake_pop_check_state()
        if(SQLITEMAN_SQLITE_HAS_COLUMN_METADATA)
            return()
        endif()
        message(STATUS "The system SQLite is built without column metadata; building a bundled copy")
    else()
        message(STATUS "SQLite ${SQLITEMAN_SQLITE_MIN_VERSION} or later not found; building a bundled copy")
    endif()
endif()

# The URL hash pins the exact archive; update both together. sqlite.org
# publishes SHA3-256 hashes on its download page.
include(FetchContent)
FetchContent_Declare(sqlite_amalgamation
    URL https://www.sqlite.org/2026/sqlite-amalgamation-3540100.zip
    URL_HASH SHA3_256=92281c86e2957f9b9305890f9ac04204caa7b677cc868dc326d942689df28e30
    DOWNLOAD_EXTRACT_TIMESTAMP ON)
FetchContent_MakeAvailable(sqlite_amalgamation)
message(STATUS "Using bundled SQLite 3.54.1")

find_package(Threads REQUIRED)

add_library(sqliteman_sqlite3 STATIC "${sqlite_amalgamation_SOURCE_DIR}/sqlite3.c")
add_library(SQLite::SQLite3 ALIAS sqliteman_sqlite3)
target_include_directories(sqliteman_sqlite3 SYSTEM PUBLIC "${sqlite_amalgamation_SOURCE_DIR}")
# What the sqlite3 shell is built with, so that a database made with it opens
# here with all its virtual tables, and column metadata for editing results.
target_compile_definitions(sqliteman_sqlite3 PRIVATE
    SQLITE_THREADSAFE=1
    SQLITE_ENABLE_COLUMN_METADATA
    SQLITE_ENABLE_DBSTAT_VTAB
    SQLITE_ENABLE_EXPLAIN_COMMENTS
    SQLITE_ENABLE_FTS4
    SQLITE_ENABLE_FTS5
    SQLITE_ENABLE_GEOPOLY
    SQLITE_ENABLE_MATH_FUNCTIONS
    SQLITE_ENABLE_RTREE
    SQLITE_ENABLE_STAT4
    SQLITE_ENABLE_UPDATE_DELETE_LIMIT)
target_link_libraries(sqliteman_sqlite3 PRIVATE Threads::Threads ${CMAKE_DL_LIBS})
if(CMAKE_SYSTEM_NAME MATCHES "Linux|BSD")
    # The math functions call libm.
    target_link_libraries(sqliteman_sqlite3 PRIVATE m)
endif()
set_target_properties(sqliteman_sqlite3 PROPERTIES
    AUTOMOC OFF AUTOUIC OFF AUTORCC OFF
    POSITION_INDEPENDENT_CODE ON)
# Third-party code: keep its warnings out of our build log.
if(MSVC)
    target_compile_options(sqliteman_sqlite3 PRIVATE /w)
else()
    target_compile_options(sqliteman_sqlite3 PRIVATE -w)
endif()
