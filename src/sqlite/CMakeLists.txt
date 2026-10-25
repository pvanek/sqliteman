# SPDX-FileCopyrightText: 2026 Petr Vanek
# SPDX-License-Identifier: GPL-3.0-or-later

add_library(sqliteman_sqlite STATIC
    Connection.cpp
    Connection.h
    Direct.cpp
    Direct.h
    Handles.h
    QueryRunner.cpp
    QueryRunner.h
    Quote.cpp
    Quote.h
    Result.cpp
    Result.h
    RowStore.cpp
    RowStore.h
    Value.cpp
    Value.h)
add_library(Sqliteman::Sqlite ALIAS sqliteman_sqlite)

target_include_directories(sqliteman_sqlite PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/..")
target_link_libraries(sqliteman_sqlite PUBLIC Qt6::Core SQLite::SQLite3)
sqliteman_target_defaults(sqliteman_sqlite)

if(SQLITEMAN_BUILD_TESTS)
    add_subdirectory(tests)
endif()
