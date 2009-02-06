IF (WANT_RESOURCES)
    MESSAGE(STATUS "Translations are not installed as you are using Qt4 resources for it")
ELSE (WANT_RESOURCES)

INSTALL(FILES
    sqliteman_en.qm
    sqliteman_bg.qm
    sqliteman_cs.qm
    sqliteman_de.qm
    sqliteman_fr.qm
    sqliteman_it.qm
    sqliteman_pl.qm
    sqliteman_ru.qm
  DESTINATION ${TRANSLATION_DIR}
)

ENDIF (WANT_RESOURCES)
