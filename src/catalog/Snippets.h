// SPDX-FileCopyrightText: 2026 Petr Vanek
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QString>

#include <vector>

namespace sqliteman::catalog {

// A short word that stands for a piece of SQL: "sf" for "SELECT * FROM".
// Completion offers these wherever a keyword would do.
struct Snippet
{
    QString abbreviation; // What is typed, and what the list shows.
    QString text; // What it becomes; CaretMark says where the caret lands.
    QString title; // What the list shows beside the abbreviation.

    // Not something anyone writes in SQL by accident.
    static constexpr QLatin1StringView CaretMark {"$|"};
};

// The ones that come with the program, in no particular order: completion
// sorts them by how well they match what was typed.
const std::vector<Snippet> &snippets();

} // namespace sqliteman::catalog
