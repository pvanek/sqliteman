// SPDX-FileCopyrightText: 2026 Petr Vanek
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QKeySequence>
#include <QList>

namespace sqliteman::Shortcuts {

// Refresh: F5 and Ctrl+R; on macOS Cmd+R (Qt::CTRL is Command there).
inline QList<QKeySequence> refresh()
{
#ifdef Q_OS_MACOS
    return {QKeySequence(Qt::CTRL | Qt::Key_R)};
#else
    return {QKeySequence(Qt::Key_F5), QKeySequence(Qt::CTRL | Qt::Key_R)};
#endif
}

} // namespace sqliteman::Shortcuts
