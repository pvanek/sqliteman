INCLUDE_DIRECTORIES(
${CMAKE_SOURCE_DIR}
${CMAKE_SOURCE_DIR}/sqliteman
)

IF (WANT_RESOURCES)
    MESSAGE(STATUS "Icons are not installed as you are using Qt4 resources for it")
ELSE (WANT_RESOURCES)

INSTALL(FILES
blob.png
clear_table_contents.png
close.png
column.png
database_commit.png
database.png
database_rollback.png
delete_table_row.png
document-export.png
document-new.png
document-open.png
document-save-as.png
document-save.png
edit.png
go-next.png
go-previous.png
index.png
insert_table_row.png
kate.png
key.png
preferences-desktop-display.png
runexplain.png
runscript.png
runsql.png
setnull.png
snapshot.png
sqliteman.png
system.png
system-search.png
table.png
trigger.png
view.png
  DESTINATION ${ICON_DIR}
)

ENDIF (WANT_RESOURCES)
