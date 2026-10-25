# SPDX-FileCopyrightText: 2026 Petr Vanek
# SPDX-License-Identifier: GPL-3.0-or-later

add_library(sqliteman_sql STATIC
    Comments.cpp
    Comments.h
    Keywords.cpp
    Keywords.h
    Lexer.cpp
    Lexer.h
    Splitter.cpp
    Splitter.h
    TextSearch.cpp
    TextSearch.h)
add_library(Sqliteman::Sql ALIAS sqliteman_sql)

target_include_directories(sqliteman_sql PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/..")
target_link_libraries(sqliteman_sql
    PUBLIC Qt6::Core
    PRIVATE SQLite::SQLite3)
sqliteman_target_defaults(sqliteman_sql)

if(SQLITEMAN_BUILD_TESTS)
    add_subdirectory(tests)
endif()
