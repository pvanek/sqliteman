// SPDX-FileCopyrightText: 2026 Petr Vanek
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

// QScintilla's headers use the slots and signals keywords, which this project
// turns off (QT_NO_KEYWORDS). Include them through this header only.
#define slots Q_SLOTS
#define signals Q_SIGNALS
#include <Qsci/qscilexercustom.h>
#include <Qsci/qsciscintilla.h>
#undef slots
#undef signals
