// SPDX-FileCopyrightText: 2026 Petr Vanek
// SPDX-License-Identifier: GPL-3.0-or-later

#include "catalog/Erd.h"

#include <QHash>

#include <algorithm>
#include <cmath>
#include <limits>
#include <ranges>

namespace sqliteman::catalog {

namespace {

// Nodes are ordered focus first, then by name: what the layout walks over
// must not depend on the order the schema happened to list them in.
void sortNodes(ErdGraph &graph)
{
    std::ranges::sort(graph.nodes, [&graph](const ErdNode &a, const ErdNode &b) {
        if ((a.id == graph.focus) != (b.id == graph.focus))
            return a.id == graph.focus;
        const int byName = a.qualifiedName().compare(b.qualifiedName(), Qt::CaseInsensitive);
        return byName != 0 ? byName < 0 : a.id < b.id;
    });
    std::ranges::sort(graph.edges, [](const ErdEdge &a, const ErdEdge &b) {
        const int byName = a.name.compare(b.name, Qt::CaseInsensitive);
        return byName != 0 ? byName < 0 : a.from < b.from;
    });
}

} // namespace

const ErdNode *ErdGraph::node(TableId id) const
{
    const auto it = std::ranges::find(nodes, id, &ErdNode::id);
    return it == nodes.end() ? nullptr : &*it;
}

const ErdPlacement *ErdLayout::placement(TableId id) const
{
    const auto it = std::ranges::find(nodes, id, &ErdPlacement::id);
    return it == nodes.end() ? nullptr : &*it;
}

std::vector<QPointF> ErdLayout::route(TableId from, TableId to) const
{
    const auto it = std::ranges::find_if(
        routes, [from, to](const ErdRoute &route) { return route.from == from && route.to == to; });
    return it == routes.end() ? std::vector<QPointF>() : it->bends;
}

bool hasDiagram(ObjectKind kind)
{
    return kind == ObjectKind::Table || kind == ObjectKind::Database;
}

ErdGraph erdGraph(const Snapshot &snapshot, ErdScope scope, const QString &schema,
                  const QString &table)
{
    ErdGraph graph;
    const DatabaseInfo *database = snapshot.database(schema);
    if (!database)
        return graph;

    // Every table of the database gets its number, in the snapshot's
    // order, whether it is drawn or not.
    std::vector<const SchemaObject *> tables;
    const auto same
        = [](const QString &a, const QString &b) { return a.compare(b, Qt::CaseInsensitive) == 0; };
    const auto idOf = [&](const QString &name) -> TableId {
        for (std::size_t i = 0; i < tables.size(); ++i) {
            if (same(tables[i]->name, name))
                return TableId(i + 1);
        }
        return 0;
    };
    for (const SchemaObject &o : database->objects) {
        if (o.kind == ObjectKind::Table || o.kind == ObjectKind::VirtualTable)
            tables.push_back(&o);
    }
    if (scope == ErdScope::Table) {
        graph.focus = idOf(table);
        if (graph.focus == 0)
            return graph;
    }

    // The edges first: for a table, they decide which neighbours there are.
    for (std::size_t i = 0; i < tables.size(); ++i) {
        const SchemaObject &child = *tables[i];
        const TableId from = TableId(i + 1);
        for (const ForeignKey &key : child.foreignKeys) {
            const TableId to = idOf(key.table);
            if (to == 0)
                continue; // A key to a table that is not there, which SQLite allows.
            if (graph.focus != 0 && from != graph.focus && to != graph.focus)
                continue;
            ErdEdge edge;
            edge.from = from;
            edge.to = to;
            edge.fromColumns = key.from;
            // No columns named: the primary key of the table referred to.
            if (key.to.isEmpty()) {
                for (const QString &column : tables[to - 1]->primaryKey())
                    edge.toColumns << column;
            } else {
                edge.toColumns = key.to;
            }
            // SQLite's keys have no names; the columns say which one it is.
            edge.name = child.name + QLatin1Char('(') + key.from.join(QLatin1String(", "))
                + QLatin1Char(')');
            edge.mandatory = std::ranges::all_of(key.from, [&](const QString &name) {
                const auto it = std::ranges::find_if(
                    child.columns, [&](const ColumnInfo &c) { return same(c.name, name); });
                return it != child.columns.end() && it->notNull;
            });
            graph.edges.push_back(std::move(edge));
        }
    }

    const auto drawn = [&](TableId id) {
        if (graph.focus == 0)
            return !tables[id - 1]->system;
        return id == graph.focus || std::ranges::any_of(graph.edges, [id](const ErdEdge &e) {
                   return e.from == id || e.to == id;
               });
    };
    for (std::size_t i = 0; i < tables.size(); ++i) {
        const TableId id = TableId(i + 1);
        if (!drawn(id))
            continue;
        const SchemaObject &o = *tables[i];
        ErdNode node;
        node.id = id;
        node.schema = o.schema;
        node.name = o.name;
        // A key column is one the table's own keys are made of: its
        // primary key, and its foreign keys' columns.
        for (const ColumnInfo &c : o.columns) {
            const bool foreign = std::ranges::any_of(o.foreignKeys, [&](const ForeignKey &k) {
                return std::ranges::any_of(k.from,
                                           [&](const QString &n) { return same(n, c.name); });
            });
            if (c.primaryKey == 0 && !foreign) {
                ++node.otherColumns;
                continue;
            }
            node.columns.push_back({c.name, c.type, c.primaryKey > 0, foreign, c.notNull});
        }
        graph.nodes.push_back(std::move(node));
    }
    // An edge to a SQLite table left out would hang in the air.
    std::erase_if(graph.edges,
                  [&graph](const ErdEdge &e) { return !graph.node(e.from) || !graph.node(e.to); });
    sortNodes(graph);
    return graph;
}

QSizeF ErdMetrics::sizeOf(const ErdNode &node) const
{
    const int rows
        = maxRows > 0 ? std::min(int(node.columns.size()), maxRows) : int(node.columns.size());
    qreal width = charWidth * qreal(node.qualifiedName().size());
    for (int row = 0; row < rows; ++row) {
        const ErdColumn &column = node.columns[std::size_t(row)];
        // The type sits right of the name, with a gap between them.
        width = std::max(width, charWidth * qreal(column.name.size() + column.type.size() + 3));
    }
    width = std::clamp(width + 2 * padding, minWidth, maxWidth);
    // A row is kept free for the line saying how many columns are not shown.
    const bool more = rows < int(node.columns.size()) || node.otherColumns > 0;
    return {width, headerHeight + rowHeight * qreal(rows + (more ? 1 : 0)) + padding};
}

namespace {

// Sugiyama, in the order the phases are usually given: break the cycles,
// put every table on a layer, decide the order within each layer, then the
// coordinates. Everything that could depend on the order rows arrived in is
// sorted by name first, so the same schema always draws the same.
class Layered
{
public:
    Layered(const ErdGraph &graph, const ErdMetrics &metrics) : m_graph(graph), m_metrics(metrics)
    { }

    ErdLayout run()
    {
        build();
        breakCycles();
        assignLayers();
        addDummies();
        ErdLayout layout;
        // Parts that are not connected to each other are laid out on their
        // own and packed afterwards; laying them out together would let one
        // part's rows push another's about for no reason.
        qreal left = 0;
        for (const std::vector<int> &component : components()) {
            const ErdLayout part = layoutComponent(component);
            for (const ErdPlacement &placement : part.nodes)
                layout.nodes.push_back({placement.id, placement.box.translated(left, 0)});
            for (ErdRoute route : part.routes) {
                for (QPointF &bend : route.bends)
                    bend.rx() += left;
                layout.routes.push_back(std::move(route));
            }
            left += part.bounds.width() + m_metrics.columnGap * 2;
        }
        std::ranges::sort(layout.nodes, {}, &ErdPlacement::id);
        for (const ErdPlacement &placement : layout.nodes)
            layout.bounds
                = layout.bounds.isNull() ? placement.box : layout.bounds.united(placement.box);
        return layout;
    }

private:
    struct Vertex
    {
        TableId id = 0; // 0 for a dummy: a place kept free for an edge to pass.
        QSizeF size;
        std::vector<int> parents; // Tables this one points at with a key.
        std::vector<int> children; // Tables pointing at this one.
        int layer = 0;
        int order = 0;
        qreal x = 0;
        qreal y = 0;
    };

    void build()
    {
        std::vector<const ErdNode *> nodes;
        for (const ErdNode &node : m_graph.nodes)
            nodes.push_back(&node);
        std::ranges::sort(nodes, [](const ErdNode *a, const ErdNode *b) {
            const int byName = a->qualifiedName().compare(b->qualifiedName(), Qt::CaseInsensitive);
            return byName != 0 ? byName < 0 : a->id < b->id;
        });
        for (const ErdNode *node : nodes) {
            m_index.insert(node->id, int(m_vertices.size()));
            Vertex vertex;
            vertex.id = node->id;
            vertex.size = m_metrics.sizeOf(*node);
            m_vertices.push_back(std::move(vertex));
        }

        for (const ErdEdge &edge : m_graph.edges) {
            if (edge.isSelfReference())
                continue; // A loop on the box: it has no say in the layers.
            const int from = m_index.value(edge.from, -1);
            const int to = m_index.value(edge.to, -1);
            if (from < 0 || to < 0 || from == to)
                continue;
            // Several keys between the same two tables are one link here.
            if (std::ranges::find(m_vertices[std::size_t(from)].parents, to)
                == m_vertices[std::size_t(from)].parents.end()) {
                m_vertices[std::size_t(from)].parents.push_back(to);
                m_vertices[std::size_t(to)].children.push_back(from);
                // Upper end first: the table being referenced is the parent.
                m_links.push_back({to, from, {}});
            }
        }
    }

    // Depth-first, reversing the edges that lead back into the stack: after
    // this the graph is acyclic, whatever the keys do.
    void breakCycles()
    {
        std::vector<int> state(m_vertices.size(), 0); // 0 new, 1 on the stack, 2 done.
        std::vector<std::pair<int, int>> back;
        auto visit = [&](auto &&self, int v) -> void {
            state[std::size_t(v)] = 1;
            for (const int parent : m_vertices[std::size_t(v)].parents) {
                if (state[std::size_t(parent)] == 1)
                    back.emplace_back(v, parent);
                else if (state[std::size_t(parent)] == 0)
                    self(self, parent);
            }
            state[std::size_t(v)] = 2;
        };
        for (int v = 0; v < int(m_vertices.size()); ++v) {
            if (state[std::size_t(v)] == 0)
                visit(visit, v);
        }
        for (const auto &[from, to] : back) {
            std::erase(m_vertices[std::size_t(from)].parents, to);
            std::erase(m_vertices[std::size_t(to)].children, from);
            m_reversed.emplace_back(from, to);
            if (std::ranges::find(m_vertices[std::size_t(to)].parents, from)
                == m_vertices[std::size_t(to)].parents.end()) {
                m_vertices[std::size_t(to)].parents.push_back(from);
                m_vertices[std::size_t(from)].children.push_back(to);
            }
        }
    }

    // Longest path: a table sits one layer below the lowest table it points
    // at, so what is referenced is always above what references it.
    void assignLayers()
    {
        std::vector<int> pending(m_vertices.size(), 0);
        std::vector<int> ready;
        for (int v = 0; v < int(m_vertices.size()); ++v) {
            pending[std::size_t(v)] = int(m_vertices[std::size_t(v)].parents.size());
            if (pending[std::size_t(v)] == 0)
                ready.push_back(v);
        }
        for (std::size_t head = 0; head < ready.size(); ++head) {
            const int v = ready[head];
            for (const int child : m_vertices[std::size_t(v)].children) {
                Vertex &c = m_vertices[std::size_t(child)];
                c.layer = std::max(c.layer, m_vertices[std::size_t(v)].layer + 1);
                if (--pending[std::size_t(child)] == 0)
                    ready.push_back(child);
            }
        }
    }

    // An edge crossing several layers would be drawn straight through the
    // tables in between. A dummy on each layer it passes keeps a channel
    // free for it, and the layout's own ordering keeps that channel tidy.
    void addDummies()
    {
        for (Link &link : m_links) {
            const int upper = m_vertices[std::size_t(link.parent)].layer;
            const int lower = m_vertices[std::size_t(link.child)].layer;
            if (lower - upper <= 1)
                continue;
            int previous = link.parent;
            for (int layer = upper + 1; layer < lower; ++layer) {
                const int dummy = int(m_vertices.size());
                Vertex channel;
                channel.size = QSizeF(1, 1);
                channel.layer = layer;
                m_vertices.push_back(std::move(channel));
                m_vertices[std::size_t(previous)].children.push_back(dummy);
                m_vertices[std::size_t(dummy)].parents.push_back(previous);
                link.dummies.push_back(dummy);
                previous = dummy;
            }
            m_vertices[std::size_t(previous)].children.push_back(link.child);
            m_vertices[std::size_t(link.child)].parents.push_back(previous);
            // The long link itself is gone: the chain of dummies replaces it.
            std::erase(m_vertices[std::size_t(link.child)].parents, link.parent);
            std::erase(m_vertices[std::size_t(link.parent)].children, link.child);
        }
    }

    std::vector<std::vector<int>> components() const
    {
        std::vector<int> seen(m_vertices.size(), 0);
        std::vector<std::vector<int>> out;
        for (int start = 0; start < int(m_vertices.size()); ++start) {
            if (seen[std::size_t(start)])
                continue;
            std::vector<int> component {start};
            seen[std::size_t(start)] = 1;
            for (std::size_t head = 0; head < component.size(); ++head) {
                const Vertex &v = m_vertices[std::size_t(component[head])];
                for (const std::vector<int> &side : {v.parents, v.children}) {
                    for (const int next : side) {
                        if (!seen[std::size_t(next)]) {
                            seen[std::size_t(next)] = 1;
                            component.push_back(next);
                        }
                    }
                }
            }
            std::ranges::sort(component); // Which is by name: that is how they were built.
            out.push_back(std::move(component));
        }
        return out;
    }

    ErdLayout layoutComponent(const std::vector<int> &component)
    {
        // The layers of this part only, each holding its vertices in order.
        int lowest = std::numeric_limits<int>::max();
        for (const int v : component)
            lowest = std::min(lowest, m_vertices[std::size_t(v)].layer);
        std::vector<std::vector<int>> layers;
        for (const int v : component) {
            const std::size_t layer = std::size_t(m_vertices[std::size_t(v)].layer - lowest);
            if (layers.size() <= layer)
                layers.resize(layer + 1);
            layers[layer].push_back(v);
        }
        for (std::vector<int> &layer : layers) {
            for (int i = 0; i < int(layer.size()); ++i)
                m_vertices[std::size_t(layer[i])].order = i;
        }

        reduceCrossings(layers);
        return coordinates(layers);
    }

    // Repeated sweeps, each putting a vertex where the middle of what it is
    // joined to is, then swapping neighbours while that helps.
    void reduceCrossings(std::vector<std::vector<int>> &layers)
    {
        constexpr int Sweeps = 6;
        for (int sweep = 0; sweep < Sweeps; ++sweep) {
            const bool downwards = sweep % 2 == 0;
            for (int i = 0; i < int(layers.size()); ++i) {
                const int index = downwards ? i : int(layers.size()) - 1 - i;
                if ((downwards && index == 0) || (!downwards && index == int(layers.size()) - 1))
                    continue;
                orderByMedian(layers[std::size_t(index)], downwards);
            }
            for (std::size_t i = 1; i < layers.size(); ++i)
                transpose(layers[i - 1], layers[i]);
        }
    }

    void orderByMedian(std::vector<int> &layer, bool byParents)
    {
        std::vector<std::pair<qreal, int>> keys;
        for (const int v : layer) {
            const std::vector<int> &linked = byParents ? m_vertices[std::size_t(v)].parents
                                                       : m_vertices[std::size_t(v)].children;
            std::vector<int> positions;
            for (const int other : linked)
                positions.push_back(m_vertices[std::size_t(other)].order);
            std::ranges::sort(positions);
            // Nothing to go by: keep where it is, so the order stays stable.
            const qreal median = positions.empty() ? qreal(m_vertices[std::size_t(v)].order)
                                                   : qreal(positions[positions.size() / 2]);
            keys.emplace_back(median, v);
        }
        std::ranges::stable_sort(keys,
                                 [](const auto &a, const auto &b) { return a.first < b.first; });
        for (int i = 0; i < int(keys.size()); ++i) {
            layer[std::size_t(i)] = keys[std::size_t(i)].second;
            m_vertices[std::size_t(keys[std::size_t(i)].second)].order = i;
        }
    }

    int crossings(const std::vector<int> &upper, const std::vector<int> &lower) const
    {
        std::vector<std::pair<int, int>> pairs;
        for (int u = 0; u < int(upper.size()); ++u) {
            for (const int child : m_vertices[std::size_t(upper[std::size_t(u)])].children) {
                const auto it = std::ranges::find(lower, child);
                if (it != lower.end())
                    pairs.emplace_back(u, int(it - lower.begin()));
            }
        }
        int count = 0;
        for (std::size_t i = 0; i < pairs.size(); ++i) {
            for (std::size_t j = i + 1; j < pairs.size(); ++j) {
                if ((pairs[i].first - pairs[j].first) * (pairs[i].second - pairs[j].second) < 0)
                    ++count;
            }
        }
        return count;
    }

    void transpose(std::vector<int> &upper, std::vector<int> &lower)
    {
        bool improved = true;
        while (improved) {
            improved = false;
            for (std::size_t i = 0; i + 1 < lower.size(); ++i) {
                std::vector<int> swapped = lower;
                std::swap(swapped[i], swapped[i + 1]);
                if (crossings(upper, swapped) < crossings(upper, lower)) {
                    lower = swapped;
                    improved = true;
                }
            }
        }
        for (int i = 0; i < int(lower.size()); ++i)
            m_vertices[std::size_t(lower[std::size_t(i)])].order = i;
    }

    // Boxes side by side in their layer, then a few passes pulling each one
    // towards the middle of what it is joined to, without letting them touch.
    ErdLayout coordinates(const std::vector<std::vector<int>> &layers)
    {
        for (const std::vector<int> &layer : layers) {
            qreal x = 0;
            for (const int v : layer) {
                m_vertices[std::size_t(v)].x = x;
                x += m_vertices[std::size_t(v)].size.width() + m_metrics.columnGap;
            }
        }
        for (int pass = 0; pass < 4; ++pass) {
            for (int i = 0; i < int(layers.size()); ++i) {
                const int index = pass % 2 == 0 ? i : int(layers.size()) - 1 - i;
                straighten(layers[std::size_t(index)], pass % 2 == 0);
            }
        }

        ErdLayout layout;
        std::vector<qreal> heights;
        qreal top = 0;
        for (const std::vector<int> &layer : layers) {
            qreal height = 0;
            for (const int v : layer) {
                Vertex &vertex = m_vertices[std::size_t(v)];
                vertex.y = top;
                // A dummy is a kept-free channel, not a table: it is drawn
                // as a bend in an edge, never as a box.
                if (vertex.id != 0) {
                    layout.nodes.push_back(
                        {vertex.id, QRectF(QPointF(vertex.x, top), vertex.size)});
                }
                height = std::max(height, vertex.size.height());
            }
            heights.push_back(height);
            top += height + m_metrics.rowGap;
        }

        // Where the edges that cross a layer have to bend: straight down the
        // channel their dummies kept free, from the layer's bottom to its
        // top, so they cross no box on the way; only the gaps between the
        // layers, where there are none, are crossed at a slant.
        const int firstLayer = layers.empty() || layers.front().empty()
            ? 0
            : m_vertices[std::size_t(layers.front().front())].layer;
        for (const Link &link : m_links) {
            if (link.dummies.empty())
                continue;
            const int layer = m_vertices[std::size_t(link.dummies.front())].layer - firstLayer;
            if (layer < 0 || layer >= int(layers.size()))
                continue; // Another part of the diagram; it is laid out there.
            ErdRoute route;
            route.from = m_vertices[std::size_t(link.child)].id;
            route.to = m_vertices[std::size_t(link.parent)].id;
            // The dummies go down from the parent; the route goes up from
            // the child, which is how the view draws it.
            for (const int dummy : std::views::reverse(link.dummies)) {
                const Vertex &v = m_vertices[std::size_t(dummy)];
                const std::size_t band = std::size_t(v.layer - firstLayer);
                route.bends.push_back({v.x, v.y + (band < heights.size() ? heights[band] : 0)});
                route.bends.push_back({v.x, v.y});
            }
            layout.routes.push_back(std::move(route));
        }

        qreal left = 0;
        for (const ErdPlacement &placement : layout.nodes)
            left = std::min(left, placement.box.left());
        for (ErdPlacement &placement : layout.nodes) {
            placement.box.translate(-left, 0);
            layout.bounds
                = layout.bounds.isNull() ? placement.box : layout.bounds.united(placement.box);
        }
        for (ErdRoute &route : layout.routes) {
            for (QPointF &bend : route.bends)
                bend.rx() -= left;
        }
        return layout;
    }

    void straighten(const std::vector<int> &layer, bool byParents)
    {
        for (int i = 0; i < int(layer.size()); ++i) {
            Vertex &v = m_vertices[std::size_t(layer[std::size_t(i)])];
            const std::vector<int> &linked = byParents ? v.parents : v.children;
            if (linked.empty())
                continue;
            std::vector<qreal> centres;
            for (const int other : linked) {
                const Vertex &o = m_vertices[std::size_t(other)];
                centres.push_back(o.x + o.size.width() / 2);
            }
            std::ranges::sort(centres);
            const qreal wanted = centres[centres.size() / 2] - v.size.width() / 2;

            // It may move only as far as its neighbours in the layer allow.
            qreal lowest = std::numeric_limits<qreal>::lowest();
            qreal highest = std::numeric_limits<qreal>::max();
            if (i > 0) {
                const Vertex &before = m_vertices[std::size_t(layer[std::size_t(i - 1)])];
                lowest = before.x + before.size.width() + m_metrics.columnGap;
            }
            if (i + 1 < int(layer.size())) {
                const Vertex &after = m_vertices[std::size_t(layer[std::size_t(i + 1)])];
                highest = after.x - v.size.width() - m_metrics.columnGap;
            }
            if (lowest <= highest)
                v.x = std::clamp(wanted, lowest, highest);
        }
    }

    struct Link
    {
        int parent = 0;
        int child = 0;
        std::vector<int> dummies; // Where it passes, layer by layer.
    };

    const ErdGraph &m_graph;
    const ErdMetrics &m_metrics;
    std::vector<Vertex> m_vertices;
    std::vector<Link> m_links;
    QHash<TableId, int> m_index;
    std::vector<std::pair<int, int>> m_reversed;
};

// Too many tables for any layout to help: name order, packed into a block.
ErdLayout gridLayout(const ErdGraph &graph, const ErdMetrics &metrics)
{
    std::vector<const ErdNode *> nodes;
    for (const ErdNode &node : graph.nodes)
        nodes.push_back(&node);
    std::ranges::sort(nodes, [](const ErdNode *a, const ErdNode *b) {
        return a->qualifiedName().compare(b->qualifiedName(), Qt::CaseInsensitive) < 0;
    });

    const int columns = std::max(1, int(std::ceil(std::sqrt(qreal(nodes.size())))));
    ErdLayout layout;
    qreal x = 0;
    qreal y = 0;
    qreal rowHeight = 0;
    for (int i = 0; i < int(nodes.size()); ++i) {
        const QSizeF size = metrics.sizeOf(*nodes[std::size_t(i)]);
        layout.nodes.push_back({nodes[std::size_t(i)]->id, QRectF(QPointF(x, y), size)});
        rowHeight = std::max(rowHeight, size.height());
        x += size.width() + metrics.columnGap;
        if ((i + 1) % columns == 0) {
            x = 0;
            y += rowHeight + metrics.rowGap;
            rowHeight = 0;
        }
    }
    for (const ErdPlacement &placement : layout.nodes)
        layout.bounds
            = layout.bounds.isNull() ? placement.box : layout.bounds.united(placement.box);
    return layout;
}

} // namespace

ErdLayout layeredLayout(const ErdGraph &graph, const ErdMetrics &metrics)
{
    if (graph.nodes.empty())
        return {};
    if (int(graph.nodes.size()) > ManyTables)
        return gridLayout(graph, metrics);
    return Layered(graph, metrics).run();
}

ErdLayout clusteredLayout(const ErdGraph &graph, const ErdMetrics &metrics)
{
    if (graph.nodes.empty())
        return {};
    if (int(graph.nodes.size()) > ManyTables)
        return layeredLayout(graph, metrics); // Which falls back to a grid.

    // One sub-graph per schema, in name order. Keys that cross a schema
    // are left out of the blocks and drawn between them afterwards.
    std::vector<QString> schemas;
    for (const ErdNode &node : graph.nodes) {
        if (std::ranges::find(schemas, node.schema) == schemas.end())
            schemas.push_back(node.schema);
    }
    std::ranges::sort(schemas, [](const QString &a, const QString &b) {
        return a.compare(b, Qt::CaseInsensitive) < 0;
    });

    struct Block
    {
        QString schema;
        ErdLayout layout;
    };
    std::vector<Block> blocks;
    qreal widest = 0;
    qreal area = 0;
    for (const QString &schema : schemas) {
        ErdGraph part;
        for (const ErdNode &node : graph.nodes) {
            if (node.schema == schema)
                part.nodes.push_back(node);
        }
        for (const ErdEdge &edge : graph.edges) {
            const ErdNode *from = graph.node(edge.from);
            const ErdNode *to = graph.node(edge.to);
            if (from && to && from->schema == schema && to->schema == schema)
                part.edges.push_back(edge);
        }
        Block block {schema, layeredLayout(part, metrics)};
        widest = std::max(widest, block.layout.bounds.width());
        area += block.layout.bounds.width() * block.layout.bounds.height();
        blocks.push_back(std::move(block));
    }

    // Blocks in rows, wrapping at roughly the width that makes the whole
    // diagram about as wide as it is tall. The tallest go first, so a row
    // is not left half empty under a short block.
    const qreal padding = metrics.padding * 3;
    const qreal label = metrics.headerHeight;
    std::ranges::stable_sort(blocks, [](const Block &a, const Block &b) {
        return a.layout.bounds.height() > b.layout.bounds.height();
    });
    const qreal target = std::max(widest, std::sqrt(area) * 2.0);
    ErdLayout layout;
    qreal x = 0;
    qreal y = 0;
    qreal rowHeight = 0;
    for (const Block &block : blocks) {
        const QSizeF size(block.layout.bounds.width() + 2 * padding,
                          block.layout.bounds.height() + 2 * padding + label);
        if (x > 0 && x + size.width() > target) {
            x = 0;
            y += rowHeight + metrics.rowGap;
            rowHeight = 0;
        }
        const QPointF offset(x + padding - block.layout.bounds.left(),
                             y + padding + label - block.layout.bounds.top());
        for (const ErdPlacement &placement : block.layout.nodes)
            layout.nodes.push_back({placement.id, placement.box.translated(offset)});
        for (ErdRoute route : block.layout.routes) {
            for (QPointF &bend : route.bends)
                bend += offset;
            layout.routes.push_back(std::move(route));
        }
        layout.clusters.push_back({block.schema, QRectF(QPointF(x, y), size)});
        x += size.width() + metrics.columnGap;
        rowHeight = std::max(rowHeight, size.height());
    }

    for (const ErdCluster &cluster : layout.clusters)
        layout.bounds = layout.bounds.isNull() ? cluster.box : layout.bounds.united(cluster.box);
    return layout;
}

ErdLayout layoutFor(const ErdGraph &graph, const ErdMetrics &metrics)
{
    if (graph.focus != 0 && graph.node(graph.focus))
        return starLayout(graph, metrics);
    // Tables of several schemas belong in frames of their own, whether the
    // diagram is of a database or of a schema whose keys reach outside it.
    const auto elsewhere = std::ranges::find_if(graph.nodes, [&graph](const ErdNode &node) {
        return node.schema != graph.nodes.front().schema;
    });
    return elsewhere == graph.nodes.end() ? layeredLayout(graph, metrics)
                                          : clusteredLayout(graph, metrics);
}

ErdLayout starLayout(const ErdGraph &graph, const ErdMetrics &metrics)
{
    ErdLayout layout;
    if (graph.nodes.empty())
        return layout;

    const ErdNode *focus = graph.node(graph.focus);
    if (!focus)
        focus = &graph.nodes.front(); // No focus given: the first node stands in.

    // Tables the focus points at go in the upper half, tables pointing at it
    // in the lower one; a table doing both goes above, where its keys are.
    std::vector<TableId> above;
    std::vector<TableId> below;
    for (const ErdEdge &edge : graph.edges) {
        if (edge.isSelfReference())
            continue; // Drawn as a loop on the box, so it needs no place.
        if (edge.from == focus->id)
            above.push_back(edge.to);
        else if (edge.to == focus->id)
            below.push_back(edge.from);
    }
    auto tidy = [&graph, focus](std::vector<TableId> &row) {
        std::ranges::sort(row, [&graph](TableId a, TableId b) {
            const ErdNode *left = graph.node(a);
            const ErdNode *right = graph.node(b);
            return left && right
                ? left->qualifiedName().compare(right->qualifiedName(), Qt::CaseInsensitive) < 0
                : a < b;
        });
        const auto duplicates = std::ranges::unique(row);
        row.erase(duplicates.begin(), duplicates.end());
        std::erase(row, focus->id);
    };
    tidy(above);
    tidy(below);

    // Tables with no key either way still belong in the picture; they go
    // out to the sides, where nothing else needs the room.
    std::vector<TableId> loose;
    for (const ErdNode &node : graph.nodes) {
        auto listed = [](const std::vector<TableId> &row, TableId id) {
            return std::ranges::find(row, id) != row.end();
        };
        if (node.id == focus->id || listed(above, node.id) || listed(below, node.id))
            continue;
        loose.push_back(node.id);
    }

    const QSizeF focusSize = metrics.sizeOf(*focus);
    QSizeF widest(0, 0);
    for (const ErdNode &node : graph.nodes) {
        const QSizeF size = metrics.sizeOf(node);
        widest = QSizeF(std::max(widest.width(), size.width()),
                        std::max(widest.height(), size.height()));
    }

    // Round the focus table rather than in rows beside it: a row of eight
    // neighbours is a diagram nobody can see at once.
    constexpr int PerRing = 5;
    constexpr qreal HalfArc = 1.15; // Radians either side of straight up or down.
    const qreal stepX = widest.width() * 0.75 + metrics.columnGap;
    const qreal stepY = widest.height() + metrics.rowGap;
    const QPointF centre(0, 0);

    auto placeAround = [&](const std::vector<TableId> &group, qreal middle, qreal halfArc) {
        const int rings = std::max(1, int(std::ceil(qreal(group.size()) / PerRing)));
        const int perRing = int(std::ceil(qreal(group.size()) / rings));
        for (int index = 0; index < int(group.size()); ++index) {
            const ErdNode *node = graph.node(group[std::size_t(index)]);
            if (!node)
                continue;
            const int ring = index / perRing;
            const int inRing = index % perRing;
            const int count = std::min(perRing, int(group.size()) - ring * perRing);
            // One neighbour sits straight above or below; more spread out
            // evenly across the arc, in the order of their names.
            const qreal fraction = count == 1 ? 0.5 : qreal(inRing) / (count - 1);
            const qreal angle = middle - halfArc + 2 * halfArc * fraction;
            const qreal radiusX = focusSize.width() / 2 + stepX * (ring + 1);
            const qreal radiusY = focusSize.height() / 2 + stepY * (ring + 1) * 0.8;
            const QSizeF size = metrics.sizeOf(*node);
            const QPointF middlePoint(centre.x() + std::cos(angle) * radiusX,
                                      centre.y() + std::sin(angle) * radiusY);
            layout.nodes.push_back(
                {node->id,
                 QRectF(middlePoint - QPointF(size.width() / 2, size.height() / 2), size)});
        }
    };

    layout.nodes.push_back(
        {focus->id,
         QRectF(centre - QPointF(focusSize.width() / 2, focusSize.height() / 2), focusSize)});
    const std::size_t focusIndex = layout.nodes.size() - 1;
    placeAround(above, -M_PI_2, HalfArc); // Up is a negative angle on screen.
    placeAround(below, M_PI_2, HalfArc);
    placeAround(loose, 0, 0.6); // Off to the right, out of everyone's way.

    // Angles alone let boxes of different sizes touch; push the pairs that
    // overlap apart, away from the middle, leaving the focus where it is.
    for (int pass = 0; pass < 40; ++pass) {
        bool moved = false;
        for (std::size_t i = 0; i < layout.nodes.size(); ++i) {
            for (std::size_t j = i + 1; j < layout.nodes.size(); ++j) {
                QRectF &a = layout.nodes[i].box;
                QRectF &b = layout.nodes[j].box;
                const QRectF gap = a.adjusted(-metrics.columnGap / 2, -metrics.rowGap / 2,
                                              metrics.columnGap / 2, metrics.rowGap / 2);
                if (!gap.intersects(b))
                    continue;
                QPointF apart = b.center() - a.center();
                if (apart.isNull())
                    apart = QPointF(1, 0);
                const qreal length = std::hypot(apart.x(), apart.y());
                const QPointF step = apart / length * (metrics.columnGap / 2 + 4);
                if (i != focusIndex)
                    a.translate(-step);
                b.translate(i == focusIndex ? step * 2 : step);
                moved = true;
            }
        }
        if (!moved)
            break;
    }

    for (const ErdPlacement &placement : layout.nodes)
        layout.bounds
            = layout.bounds.isNull() ? placement.box : layout.bounds.united(placement.box);
    // The scene starts at the origin, whatever side of the focus things fell.
    const QPointF shift = -layout.bounds.topLeft();
    for (ErdPlacement &placement : layout.nodes)
        placement.box.translate(shift);
    layout.bounds.translate(shift);
    return layout;
}

} // namespace sqliteman::catalog
