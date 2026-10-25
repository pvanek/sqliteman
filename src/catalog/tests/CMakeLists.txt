# SPDX-FileCopyrightText: 2026 Petr Vanek
# SPDX-License-Identifier: GPL-3.0-or-later

foreach(test
        tst_altertable
        tst_completion
        tst_csv
        tst_details
        tst_dump
        tst_editing
        tst_erd
        tst_erdexport
        tst_export
        tst_import
        tst_plan
        tst_populator
        tst_pragmas
        tst_querybuilder
        tst_snapshot
        tst_tabledefinition
        tst_valueformat)
    sqliteman_add_test(${test} Sqliteman::Catalog)
endforeach()
