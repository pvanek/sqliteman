// SPDX-FileCopyrightText: 2026 Petr Vanek
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <sqlite3.h>

#include <memory>

namespace sqliteman::sqlite {

struct StatementDeleter
{
    void operator()(sqlite3_stmt *stmt) const { sqlite3_finalize(stmt); }
};
using StatementPtr = std::unique_ptr<sqlite3_stmt, StatementDeleter>;

} // namespace sqliteman::sqlite
