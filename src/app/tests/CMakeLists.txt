# SPDX-FileCopyrightText: 2026 Petr Vanek
# SPDX-License-Identifier: GPL-3.0-or-later

sqliteman_add_gui_test(tst_resultmodel sqliteman_ui)
sqliteman_add_gui_test(tst_sqleditor sqliteman_ui)
sqliteman_add_gui_test(tst_editorpage sqliteman_ui)
sqliteman_add_gui_test(tst_browser sqliteman_ui)
sqliteman_add_gui_test(tst_dialogs sqliteman_ui)
sqliteman_add_gui_test(tst_mainwindow sqliteman_ui)

# Software centres and Flathub reject an invalid desktop or metainfo file,
# and the packaging only finds out after a full build.
find_program(SQLITEMAN_DESKTOP_FILE_VALIDATE desktop-file-validate)
if(SQLITEMAN_DESKTOP_FILE_VALIDATE)
    add_test(NAME desktop_file
        COMMAND "${SQLITEMAN_DESKTOP_FILE_VALIDATE}"
            "${CMAKE_CURRENT_SOURCE_DIR}/../cz.yarpen.sqliteman.desktop")
endif()
find_program(SQLITEMAN_APPSTREAMCLI appstreamcli)
if(SQLITEMAN_APPSTREAMCLI)
    add_test(NAME metainfo_file
        COMMAND "${SQLITEMAN_APPSTREAMCLI}" validate --no-net
            "${SQLITEMAN_METAINFO}")
endif()
