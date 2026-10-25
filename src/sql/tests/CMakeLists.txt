# SPDX-FileCopyrightText: 2026 Petr Vanek
# SPDX-License-Identifier: GPL-3.0-or-later

sqliteman_add_test(tst_lexer Sqliteman::Sql)
sqliteman_add_test(tst_splitter Sqliteman::Sql SQLite::SQLite3)
sqliteman_add_test(tst_comments Sqliteman::Sql)
sqliteman_add_test(tst_textsearch Sqliteman::Sql)
