# SPDX-FileCopyrightText: 2026 Petr Vanek
# SPDX-License-Identifier: GPL-3.0-or-later

# The manual's screenshots; run.sh builds and runs this.
add_executable(sqliteman_screenshots screenshots.cpp)
target_link_libraries(sqliteman_screenshots PRIVATE sqliteman_ui Qt6::Test)
# The program's icon, which the sample database keeps as a picture.
qt_add_resources(sqliteman_screenshots "icons" PREFIX "/"
    BASE "${CMAKE_SOURCE_DIR}/src/app"
    FILES "${CMAKE_SOURCE_DIR}/src/app/icons/sqliteman-128.png")
sqliteman_target_defaults(sqliteman_screenshots)
