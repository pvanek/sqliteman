# SPDX-FileCopyrightText: 2026 Petr Vanek
# SPDX-License-Identifier: GPL-3.0-or-later

add_library(sqliteman_config STATIC
    Preferences.cpp
    Preferences.h
    RecentFiles.cpp
    RecentFiles.h)
add_library(Sqliteman::Config ALIAS sqliteman_config)

target_include_directories(sqliteman_config PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/..")
# QColor and QFont, kept as values; nothing that needs a display.
target_link_libraries(sqliteman_config PUBLIC Qt6::Gui)
sqliteman_target_defaults(sqliteman_config)

if(SQLITEMAN_BUILD_TESTS)
    add_subdirectory(tests)
endif()
