// SPDX-FileCopyrightText: 2026 Petr Vanek
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "catalog/Snapshot.h"

#include <QIcon>

// Icons from the desktop's icon theme, with Qt's built-in ones as fallback.
namespace sqliteman::Icons {

// An open database file, a dot in its color; filled in when it is open.
QIcon database(bool open);
QIcon object(catalog::ObjectKind kind);
QIcon folder();
QIcon system(); // SQLite's own tables.
QIcon primaryKey();
QIcon error();
QIcon refresh();
QIcon close();
QIcon run();
QIcon runScript();
QIcon explain();
QIcon stop();
// Tables and the foreign keys between them.
QIcon diagram();
QIcon commit();
QIcon rollback();
QIcon add();
QIcon remove();
QIcon edit();
QIcon exportData();
QIcon importData();
// A database with work in progress: amber while a transaction is open;
// greyed out (disabled), no transaction.
QIcon transaction();

} // namespace sqliteman::Icons
