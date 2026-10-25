// SPDX-FileCopyrightText: 2026 Petr Vanek
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "catalog/Snapshot.h"

#include <QByteArray>
#include <QPointF>
#include <QRectF>
#include <QSizeF>
#include <QString>
#include <QStringList>

#include <vector>

namespace sqliteman::catalog {

// A table's number in one graph, given as the graph is built: SQLite has no
// object ids, and names would make every lookup a case-insensitive compare.
using TableId = unsigned int;

// Tables and the foreign keys between them, as a diagram needs them: no Qt
// Widgets anywhere near, so the layout can be tested without a display.

struct ErdColumn
{
    QString name;
    QString type;
    bool primaryKey = false;
    bool foreignKey = false;
    bool notNull = false;
};

struct ErdNode
{
    TableId id = 0;
    QString schema;
    QString name;
    // Only the columns a key is made of: everything else is noise in a
    // diagram, and there can be a hundred of them.
    std::vector<ErdColumn> columns;
    int otherColumns = 0; // How many were left out.

    QString qualifiedName() const { return schema + QLatin1Char('.') + name; }
};

struct ErdEdge
{
    QString name; // The constraint's.
    TableId from = 0; // The table with the foreign key...
    TableId to = 0; // ...and the one it points at.
    QStringList fromColumns;
    QStringList toColumns;
    // Every column of the key is NOT NULL, so each row must have its
    // counterpart: "exactly one" rather than "zero or one" on the diagram.
    bool mandatory = false;

    bool isSelfReference() const { return from == to; }
};

struct ErdGraph
{
    TableId focus = 0; // The table the diagram was opened for, 0 for none.
    std::vector<ErdNode> nodes;
    std::vector<ErdEdge> edges;

    const ErdNode *node(TableId id) const;
    bool isEmpty() const { return nodes.empty(); }
};

// Whether a diagram is worth showing for this kind: a table, which can
// have foreign keys, or a database, which can have tables.
bool hasDiagram(ObjectKind kind);

// What a diagram covers.
enum class ErdScope {
    Table, // One table and its neighbours: what it references and what references it.
    Database // Every table of a database: main, temp or an attached one.
};

// The graph of a table of schema, or of the whole of schema, from what the
// snapshot knows: nothing is queried. SQLite's foreign keys never leave
// their database, so neither does the graph. SQLite's own tables are left
// out, unless the table asked for is one of them.
ErdGraph erdGraph(const Snapshot &snapshot, ErdScope scope, const QString &schema,
                  const QString &table = {});

// What a table box looks like, in scene units. The view fills this in from
// its own font; the defaults are here so layout tests need no font at all.
struct ErdMetrics
{
    qreal headerHeight = 26;
    qreal rowHeight = 18;
    qreal padding = 8;
    qreal minWidth = 140;
    qreal maxWidth = 280;
    qreal charWidth = 7; // Average character width of the column font.
    qreal columnGap = 40;
    qreal rowGap = 90;
    int maxRows = 0; // Columns to show at most; 0 shows every one.

    QSizeF sizeOf(const ErdNode &node) const;
};

struct ErdPlacement
{
    TableId id = 0;
    QRectF box;
};

// Where an edge has to bend to get past the tables between its ends: the
// points come from the channels the layout kept free for it, a bottom and a
// top for each layer passed, in order from the edge's from to its to.
struct ErdRoute
{
    TableId from = 0;
    TableId to = 0;
    std::vector<QPointF> bends;
};

// A schema's tables, kept together and framed, in a diagram that spans more
// than one schema.
struct ErdCluster
{
    QString name;
    QRectF box;
};

struct ErdLayout
{
    std::vector<ErdPlacement> nodes;
    std::vector<ErdRoute> routes;
    std::vector<ErdCluster> clusters;
    QRectF bounds;

    const ErdPlacement *placement(TableId id) const;
    // Empty when the edge runs between neighbouring layers, which needs no
    // help, or when the layout draws no routes at all.
    std::vector<QPointF> route(TableId from, TableId to) const;
};

// The focus table in the middle, what it references in an arc above, what
// references it in an arc below. Deterministic: the same graph always comes
// out the same way, whatever order the schema listed things in.
ErdLayout starLayout(const ErdGraph &graph, const ErdMetrics &metrics = {});

// Tables in layers, a referenced table above the ones referencing it: cycles
// broken, layers assigned by longest path, crossings cut down by repeated
// median sweeps, and disconnected parts packed side by side. Deterministic,
// like starLayout(). Above ManyTables it falls back to a plain grid, which
// is all a diagram that size is good for anyway.
ErdLayout layeredLayout(const ErdGraph &graph, const ErdMetrics &metrics = {});
constexpr int ManyTables = 150;

// Each schema laid out on its own and framed, the blocks packed into rows:
// a whole database is a set of diagrams rather than one.
ErdLayout clusteredLayout(const ErdGraph &graph, const ErdMetrics &metrics = {});

// The layout that suits the graph: a focused table gets a star, one schema
// gets layers, several schemas get a frame each.
ErdLayout layoutFor(const ErdGraph &graph, const ErdMetrics &metrics = {});

} // namespace sqliteman::catalog
