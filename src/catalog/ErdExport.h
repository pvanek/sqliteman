// SPDX-FileCopyrightText: 2026 Petr Vanek
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "catalog/Erd.h"

#include <QString>

namespace sqliteman::catalog {

// A diagram as text other tools draw: the graph only, since they lay it out
// themselves. Every column of the graph is listed, not the view's first few.

// Graphviz: one HTML-like table per node, crow's feet on the edges, a
// cluster per schema when there is more than one.
QString erdToDot(const ErdGraph &graph);

// Mermaid's erDiagram. Its names allow fewer characters than SQLite's,
// so entities get an id and the real name as their label, and columns and
// types are spelled with what Mermaid accepts.
QString erdToMermaid(const ErdGraph &graph);

} // namespace sqliteman::catalog
