# SPDX-FileCopyrightText: 2026 Petr Vanek
# SPDX-License-Identifier: GPL-3.0-or-later

# Everything but main(), so the tests can use it too.
add_library(sqliteman_ui STATIC
    CompletionPopup.cpp
    CompletionPopup.h
    DatabaseBrowser.cpp
    DatabaseBrowser.h
    DiagramFile.cpp
    DiagramFile.h
    EditorPage.cpp
    EditorPage.h
    ErdView.cpp
    ErdView.h
    ExportDialog.cpp
    ExportDialog.h
    ExportDialog.ui
    FileBrowser.cpp
    FileBrowser.h
    HelpWindow.cpp
    HelpWindow.h
    Icons.cpp
    Icons.h
    ImportDialog.cpp
    ImportDialog.h
    ImportDialog.ui
    IndexDialog.cpp
    IndexDialog.h
    IndexDialog.ui
    MainWindow.cpp
    MainWindow.h
    MaintenanceDialog.cpp
    MaintenanceDialog.h
    MaintenanceDialog.ui
    ObjectPage.cpp
    ObjectPage.h
    PageTabWidget.cpp
    PageTabWidget.h
    PageWindow.cpp
    PageWindow.h
    PlanView.cpp
    PlanView.h
    PopulatorDialog.cpp
    PopulatorDialog.h
    PopulatorDialog.ui
    PragmaPage.cpp
    PragmaPage.h
    PreferencesDialog.cpp
    PreferencesDialog.h
    PreferencesDialog.ui
    Qsci.h
    QueryBuilderDialog.cpp
    QueryBuilderDialog.h
    QueryBuilderDialog.ui
    ResultModel.cpp
    ResultModel.h
    ResultPanel.cpp
    ResultPanel.h
    ResultTextView.cpp
    ResultTextView.h
    ResultView.cpp
    ResultView.h
    SaveChangesDialog.cpp
    SaveChangesDialog.h
    SaveChangesDialog.ui
    SearchBar.cpp
    SearchBar.h
    SearchBar.ui
    Session.cpp
    Session.h
    Settings.cpp
    Settings.h
    Shortcuts.h
    SqlEditor.cpp
    SqlEditor.h
    SqlLexer.cpp
    SqlLexer.h
    SqlObjectDialog.cpp
    SqlObjectDialog.h
    SqlObjectDialog.ui
    SvgDevice.cpp
    SvgDevice.h
    TableDialog.cpp
    TableDialog.h
    TableDialog.ui
    TableData.cpp
    TableData.h
    ValueLexer.cpp
    ValueLexer.h
    ValueViewer.cpp
    ValueViewer.h
    ValueViewer.ui
    WorkspacePage.h)
target_include_directories(sqliteman_ui PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(sqliteman_ui PUBLIC
    Qt6::Widgets
    Qt6::Concurrent
    QScintilla::QScintilla
    Sqliteman::Sqlite
    Sqliteman::Config
    Sqliteman::Sql
    Sqliteman::Catalog)
if(Qt6Help_FOUND)
    target_link_libraries(sqliteman_ui PUBLIC Qt6::Help)
    target_compile_definitions(sqliteman_ui PUBLIC SQLITEMAN_WITH_HELP)
endif()
sqliteman_target_defaults(sqliteman_ui)

# The application icon: PNGs rendered from icons/sqliteman.svg by
# icons/generate.sh, which also makes the Windows .ico.
set(sqliteman_sources main.cpp)
if(WIN32)
    list(APPEND sqliteman_sources sqliteman.rc)
endif()
qt_add_executable(sqliteman ${sqliteman_sources})
set(sqliteman_icons icons/sqliteman.svg)
foreach(size 16 22 24 32 48 64 128 256 512)
    list(APPEND sqliteman_icons icons/sqliteman-${size}.png)
endforeach()
qt_add_resources(sqliteman "icons" PREFIX "/" FILES ${sqliteman_icons})
target_compile_definitions(sqliteman PRIVATE
    SQLITEMAN_VERSION="${PROJECT_VERSION}")
target_link_libraries(sqliteman PRIVATE sqliteman_ui)
sqliteman_target_defaults(sqliteman)

set_target_properties(sqliteman PROPERTIES
    WIN32_EXECUTABLE ON
    MACOSX_BUNDLE ON
    MACOSX_BUNDLE_BUNDLE_NAME Sqliteman
    MACOSX_BUNDLE_GUI_IDENTIFIER cz.yarpen.sqliteman
    MACOSX_BUNDLE_BUNDLE_VERSION "${PROJECT_VERSION}"
    MACOSX_BUNDLE_SHORT_VERSION_STRING "${PROJECT_VERSION}"
    MACOSX_BUNDLE_COPYRIGHT "Copyright 2007-2026 Petr Vanek, GPL-3.0-or-later"
    MACOSX_BUNDLE_ICON_FILE sqliteman.icns)

if(APPLE)
    # iconutil comes with every Mac, so the .icns is made from the PNGs at
    # build time instead of being one more rendered file in the repository.
    set(iconset "${CMAKE_CURRENT_BINARY_DIR}/sqliteman.iconset")
    set(icns "${CMAKE_CURRENT_BINARY_DIR}/sqliteman.icns")
    set(iconset_commands COMMAND ${CMAKE_COMMAND} -E make_directory "${iconset}")
    foreach(pair 16:16x16 32:16x16@2x 32:32x32 64:32x32@2x 128:128x128 256:128x128@2x
                 256:256x256 512:256x256@2x 512:512x512)
        string(REPLACE ":" ";" pair "${pair}")
        list(GET pair 0 size)
        list(GET pair 1 name)
        list(APPEND iconset_commands COMMAND ${CMAKE_COMMAND} -E copy
            "${CMAKE_CURRENT_SOURCE_DIR}/icons/sqliteman-${size}.png"
            "${iconset}/icon_${name}.png")
    endforeach()
    add_custom_command(OUTPUT "${icns}"
        ${iconset_commands}
        COMMAND iconutil --convert icns --output "${icns}" "${iconset}"
        DEPENDS ${sqliteman_icons}
        COMMENT "Making the macOS application icon"
        VERBATIM)
    target_sources(sqliteman PRIVATE "${icns}")
    set_source_files_properties("${icns}" PROPERTIES MACOSX_PACKAGE_LOCATION Resources)
endif()

# The version and the release date come from the top-level CMakeLists.txt.
set(SQLITEMAN_METAINFO "${CMAKE_CURRENT_BINARY_DIR}/cz.yarpen.sqliteman.metainfo.xml")
configure_file(cz.yarpen.sqliteman.metainfo.xml.in "${SQLITEMAN_METAINFO}" @ONLY)

include(GNUInstallDirs)
install(TARGETS sqliteman
    BUNDLE DESTINATION .
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
if(UNIX AND NOT APPLE)
    install(FILES cz.yarpen.sqliteman.desktop DESTINATION ${CMAKE_INSTALL_DATADIR}/applications)
    install(FILES "${SQLITEMAN_METAINFO}" DESTINATION ${CMAKE_INSTALL_DATADIR}/metainfo)
    install(FILES icons/sqliteman.svg
        DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/scalable/apps
        RENAME cz.yarpen.sqliteman.svg)
    foreach(size 16 22 24 32 48 64 128 256 512)
        install(FILES icons/sqliteman-${size}.png
            DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/${size}x${size}/apps
            RENAME cz.yarpen.sqliteman.png)
    endforeach()
endif()

if(SQLITEMAN_BUILD_TESTS)
    add_subdirectory(tests)
endif()
