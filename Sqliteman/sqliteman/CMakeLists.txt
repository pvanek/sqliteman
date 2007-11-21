ADD_SUBDIRECTORY(icons)
ADD_SUBDIRECTORY(doc)
ADD_SUBDIRECTORY(ts)


SET( SQLITEMAN_SRC
    altertabledialog.cpp
    altertriggerdialog.cpp
    alterviewdialog.cpp
    analyzedialog.cpp
    blobpreviewwidget.cpp
    constraintsdialog.cpp
    createindexdialog.cpp
    createtabledialog.cpp
    createtriggerdialog.cpp
    createviewdialog.cpp
    database.cpp
    dataexportdialog.cpp
    dataviewer.cpp
    helpbrowser.cpp
    importtabledialog.cpp
    importtablelogdialog.cpp
    multieditdialog.cpp
    litemanwindow.cpp
    main.cpp
    populatordialog.cpp
    preferences.cpp
    preferencesdialog.cpp
    queryeditordialog.cpp
    schemabrowser.cpp
    shortcuteditordialog.cpp
    shortcutmodel.cpp
    sqldelegate.cpp
    sqleditor.cpp
    sqleditorwidget.cpp
    sqliteprocess.cpp
    sqlitemview.cpp
    sqlkeywords.cpp
    sqlparser.cpp
    sqlmodels.cpp
    tableeditordialog.cpp
    tabletree.cpp
    vacuumdialog.cpp
    utils.cpp
)

# compiled in icons for windows
IF (WIN32)
    SET (SQLITEMAN_SRC ${SQLITEMAN_SRC} icons/sqliteman.rc)
ENDIF (WIN32)

SET( SQLITEMAN_MOC
    altertabledialog.h
    altertriggerdialog.h
    alterviewdialog.h
    analyzedialog.h
    blobpreviewwidget.h
    constraintsdialog.h
    createindexdialog.h
    createtabledialog.h
    createtriggerdialog.h
    createviewdialog.h
    dataexportdialog.h
    dataviewer.h
    helpbrowser.h
    importtabledialog.h
    importtablelogdialog.h
    litemanwindow.h
    multieditdialog.h
    populatordialog.h
    preferences.h
    preferencesdialog.h
    queryeditordialog.h
    schemabrowser.h
    shortcuteditordialog.h
    shortcutmodel.h
    sqldelegate.h
    sqleditor.h
    sqleditorwidget.h
    sqliteprocess.h
    sqlitemview.h
    sqlparser.h
    sqlmodels.h
    tableeditordialog.h
    tabletree.h
    vacuumdialog.h
)

SET( SQLITEMAN_UI
    analyzedialog.ui
    constraintsdialog.ui
    createindexdialog.ui
    createtriggerdialog.ui
    createviewdialog.ui
    dataexportdialog.ui
    dataviewer.ui
    helpbrowser.ui
    importtabledialog.ui
    importtablelogdialog.ui
    multieditdialog.ui
    populatordialog.ui
    preferencesdialog.ui
    prefsdatadisplaywidget.ui
    prefslnfwidget.ui
    prefssqleditorwidget.ui
    schemabrowser.ui
    shortcuteditordialog.ui
    sqldelegateui.ui
    sqleditor.ui
    sqlitemview.ui
    tableeditordialog.ui
    vacuumdialog.ui
)

IF (WANT_RESOURCES)
    SET( SQLITEMAN_RCS
        doc/doc.qrc
        icons/icons.qrc
        ts/ts.qrc
    )
    QT4_ADD_RESOURCES( SQLITEMAN_RC_RCS ${SQLITEMAN_RCS} )
ENDIF (WANT_RESOURCES)


QT4_WRAP_UI( SQLITEMAN_UI_HDRS ${SQLITEMAN_UI} )

QT4_WRAP_CPP( SQLITEMAN_MOC_SRC ${SQLITEMAN_MOC} )

INCLUDE_DIRECTORIES( ${CMAKE_BINARY_DIR} )
INCLUDE_DIRECTORIES( ${CMAKE_BINARY_DIR}/sqliteman )
# due the "out of base directory" build. E.g. SQLITEMAN_SVN_DIR/mybuild etc.
INCLUDE_DIRECTORIES( . )

ADD_EXECUTABLE( sqliteman ${SQLITEMAN_SRC} ${SQLITEMAN_MOC_SRC}
                ${SQLITEMAN_UI_HDRS} ${SQLITEMAN_RC_RCS}
)

TARGET_LINK_LIBRARIES( sqliteman ${QT_LIBRARIES}) # ${POPULATOR_LIB})
IF (WIN32)
    TARGET_LINK_LIBRARIES(sqliteman "-mwindows")
ENDIF (WIN32)    

# compress it
# IF (SELF_PACKER_FOR_EXECUTABLE)
#     ADD_CUSTOM_COMMAND(
#            SOURCE sqliteman
#            COMMAND ${SELF_PACKER_FOR_EXECUTABLE}
#            ARGS ${SELF_PACKER_FOR_EXECUTABLE_FLAGS}
#                 sqliteman
#            TARGET sqliteman)
# ENDIF (SELF_PACKER_FOR_EXECUTABLE)

IF (WIN32)
    INSTALL(TARGETS sqliteman RUNTIME DESTINATION .)
ELSE (WIN32)
    INSTALL(TARGETS sqliteman RUNTIME DESTINATION bin LIBRARY DESTINATION lib)
ENDIF (WIN32)
