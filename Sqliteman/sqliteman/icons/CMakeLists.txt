INCLUDE_DIRECTORIES(
${CMAKE_SOURCE_DIR}
${CMAKE_SOURCE_DIR}/sqliteman
)

IF (WANT_RESOURCES)
    MESSAGE(STATUS "Icons are not installed as you are using Qt4 resources for it")
ELSE (WANT_RESOURCES)

INSTALL(FILES
    clear_table_contents.png
    column.png
    document-new.png
    document-save.png
    edit.png
    key.png
    sqliteman.png
    trigger.png
    database.png
    document-open.png
    index.png
    runexplain.png
    setnull.png
    system.png
    view.png
    delete_table_row.png
    document-save-as.png
    insert_table_row.png
    runsql.png
    runscript.png
    setnull.png
    table.png
    database_commit.png
    database_rollback.png
    preferences-desktop-display.png
    kate.png
  DESTINATION ${ICON_DIR}
)

ENDIF (WANT_RESOURCES)
