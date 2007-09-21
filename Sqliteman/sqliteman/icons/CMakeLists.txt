INCLUDE_DIRECTORIES(
${CMAKE_SOURCE_DIR}
${CMAKE_SOURCE_DIR}/sqliteman
)

INSTALL(FILES
    clear_table_contents.png
    column.png
    document-new.png
    document-save.png
    key.png
    sqliteman.png
    trigger.png
    database.png
    document-open.png
    index.png
    runexplain.png
    system.png
    view.png
    delete_table_row.png
    document-save-as.png
    insert_table_row.png
    runsql.png
    table.png
    database_commit.png
    database_rollback.png
  DESTINATION ${ICON_DIR}
)