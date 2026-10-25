# SPDX-FileCopyrightText: 2026 Petr Vanek
# SPDX-License-Identifier: GPL-3.0-or-later

add_library(sqliteman_catalog STATIC
    AlterTable.cpp
    AlterTable.h
    Completion.cpp
    Completion.h
    Details.cpp
    Details.h
    Dump.cpp
    Dump.h
    Editing.cpp
    Editing.h
    Erd.cpp
    Erd.h
    ErdExport.cpp
    ErdExport.h
    Export.cpp
    Export.h
    Import.cpp
    Import.h
    Plan.cpp
    Plan.h
    Populator.cpp
    Populator.h
    Pragmas.cpp
    Pragmas.h
    QueryBuilder.cpp
    QueryBuilder.h
    Snapshot.cpp
    Snapshot.h
    Snippets.cpp
    Snippets.h
    TableDefinition.cpp
    TableDefinition.h
    ValueFormat.cpp
    ValueFormat.h)
add_library(Sqliteman::Catalog ALIAS sqliteman_catalog)

target_include_directories(sqliteman_catalog PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/..")
target_link_libraries(sqliteman_catalog
    PUBLIC Qt6::Core Sqliteman::Sqlite Sqliteman::Sql)
sqliteman_target_defaults(sqliteman_catalog)

if(SQLITEMAN_BUILD_TESTS)
    add_subdirectory(tests)
endif()
