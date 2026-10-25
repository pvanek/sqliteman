// SPDX-FileCopyrightText: 2026 Petr Vanek
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QByteArray>

namespace sqliteman::sql {

// Comments whole lines out with "-- ", or back in when every line that is
// not blank already starts with "--". The marks go at the indentation of the
// least indented line, so a commented block keeps its shape. Blank lines are
// left alone.
QByteArray toggleLineComments(const QByteArray &lines);

} // namespace sqliteman::sql
