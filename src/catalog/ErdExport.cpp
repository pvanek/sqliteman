// SPDX-FileCopyrightText: 2026 Petr Vanek
// SPDX-License-Identifier: GPL-3.0-or-later

#include "catalog/ErdExport.h"

#include <QHash>
#include <QRegularExpression>
#include <QSet>

#include <algorithm>

namespace sqliteman::catalog {

namespace {

QStringList schemasOf(const ErdGraph &graph)
{
    QStringList schemas;
    for (const ErdNode &node : graph.nodes) {
        if (!schemas.contains(node.schema))
            schemas << node.schema;
    }
    return schemas;
}

// --- Graphviz ---

QString dotString(const QString &text)
{
    QString quoted = text;
    quoted.replace(QLatin1Char('\\'), QLatin1String("\\\\"));
    quoted.replace(QLatin1Char('"'), QLatin1String("\\\""));
    return QLatin1Char('"') + quoted + QLatin1Char('"');
}

QString dotNode(TableId id)
{
    return QStringLiteral("t%1").arg(id);
}

// The port of the row a column is listed on, so an edge ends at its key's
// row rather than anywhere on the table.
QString dotPort(const ErdGraph &graph, TableId id, const QStringList &columns)
{
    const ErdNode *node = graph.node(id);
    if (!node || columns.isEmpty())
        return dotNode(id);
    const auto it = std::ranges::find(node->columns, columns.first(), &ErdColumn::name);
    if (it == node->columns.end())
        return dotNode(id);
    return dotNode(id) + QStringLiteral(":c%1").arg(it - node->columns.begin());
}

QString dotLabel(const ErdNode &node, bool focus, bool clustered)
{
    const QString name = (clustered ? node.name : node.qualifiedName()).toHtmlEscaped();
    QString label = QStringLiteral("<<TABLE BORDER=\"%1\" CELLBORDER=\"0\" CELLSPACING=\"0\" "
                                   "CELLPADDING=\"4\">\n")
                        .arg(focus ? 2 : 1);
    label += QStringLiteral("    <TR><TD COLSPAN=\"2\" BGCOLOR=\"%1\"><B>%2</B></TD></TR>\n")
                 .arg(focus ? QStringLiteral("#c6dbef") : QStringLiteral("#ececec"), name);
    for (std::size_t i = 0; i < node.columns.size(); ++i) {
        const ErdColumn &column = node.columns[i];
        QString columnName = column.name.toHtmlEscaped();
        if (column.foreignKey)
            columnName = QStringLiteral("<I>%1</I>").arg(columnName);
        if (column.primaryKey)
            columnName = QStringLiteral("<B>%1</B>").arg(columnName);
        label += QStringLiteral("    <TR><TD ALIGN=\"LEFT\" PORT=\"c%1\">%2</TD>"
                                "<TD ALIGN=\"RIGHT\"><FONT COLOR=\"#666666\">%3</FONT></TD></TR>\n")
                     .arg(i)
                     .arg(columnName, column.type.toHtmlEscaped());
    }
    if (node.otherColumns > 0) {
        label += QStringLiteral("    <TR><TD COLSPAN=\"2\" ALIGN=\"LEFT\"><FONT COLOR=\"#666666\">"
                                "<I>+ %1 %2</I></FONT></TD></TR>\n")
                     .arg(node.otherColumns)
                     .arg(node.otherColumns == 1 ? QStringLiteral("column")
                                                 : QStringLiteral("columns"));
    }
    label += QStringLiteral("</TABLE>>");
    return label;
}

// --- Mermaid ---

// What Mermaid takes as an attribute's name or type: a letter or an
// underscore, then letters, digits and a few signs. Anything else becomes
// an underscore.
QString mermaidWord(const QString &text)
{
    static const QRegularExpression notAllowed(QStringLiteral("[^A-Za-z0-9_\\-\\[\\]\\(\\)]"));
    QString word = text;
    word.replace(notAllowed, QStringLiteral("_"));
    if (word.isEmpty() || !(word.front().isLetter() || word.front() == QLatin1Char('_')))
        word.prepend(QLatin1Char('_'));
    return word;
}

// Inside its quotes Mermaid has no escape for a quote but an entity code.
QString mermaidString(const QString &text)
{
    QString quoted = text;
    quoted.replace(QLatin1Char('"'), QLatin1String("#quot;"));
    return QLatin1Char('"') + quoted + QLatin1Char('"');
}

} // namespace

QString erdToDot(const ErdGraph &graph)
{
    const QStringList schemas = schemasOf(graph);
    const bool clustered = schemas.size() > 1;

    QString dot;
    dot += QStringLiteral("digraph erd {\n");
    // Bottom to top: a referenced table above the ones referencing it, as
    // in the program's own diagram.
    dot += QStringLiteral("    graph [rankdir=BT, nodesep=0.5, ranksep=0.8, "
                          "fontname=\"Helvetica\", fontsize=11];\n");
    dot += QStringLiteral("    node [shape=plaintext, fontname=\"Helvetica\", fontsize=10];\n");
    dot += QStringLiteral("    edge [dir=both, color=\"#666666\", arrowsize=0.9];\n");

    auto writeNode = [&](const ErdNode &node, const QString &indent) {
        dot += indent + dotNode(node.id) + QStringLiteral(" [tooltip=")
            + dotString(node.qualifiedName()) + QStringLiteral(", label=")
            + dotLabel(node, node.id == graph.focus, clustered) + QStringLiteral("];\n");
    };
    if (clustered) {
        for (int i = 0; i < schemas.size(); ++i) {
            dot += QStringLiteral("\n    subgraph cluster_%1 {\n").arg(i);
            dot += QStringLiteral("        label=%1; style=dashed; color=\"#999999\";\n")
                       .arg(dotString(schemas[i]));
            for (const ErdNode &node : graph.nodes) {
                if (node.schema == schemas[i])
                    writeNode(node, QStringLiteral("        "));
            }
            dot += QStringLiteral("    }\n");
        }
    } else {
        dot += QLatin1Char('\n');
        for (const ErdNode &node : graph.nodes)
            writeNode(node, QStringLiteral("    "));
    }

    dot += QLatin1Char('\n');
    for (const ErdEdge &edge : graph.edges) {
        if (!graph.node(edge.from) || !graph.node(edge.to))
            continue;
        // Crow's foot at the referencing table, one or zero-or-one at the
        // referenced one; Graphviz draws the first shape next to the node.
        dot += QStringLiteral("    %1 -> %2 [arrowtail=crowodot, arrowhead=%3, tooltip=%4];\n")
                   .arg(dotPort(graph, edge.from, edge.fromColumns),
                        dotPort(graph, edge.to, edge.toColumns),
                        edge.mandatory ? QStringLiteral("teetee") : QStringLiteral("teeodot"),
                        dotString(edge.name));
    }
    dot += QStringLiteral("}\n");
    return dot;
}

QString erdToMermaid(const ErdGraph &graph)
{
    // Ids made from the names, so the source reads well; two names that
    // come out the same get the id to tell them apart.
    QHash<TableId, QString> ids;
    QSet<QString> taken;
    static const QRegularExpression notAllowed(QStringLiteral("[^A-Za-z0-9_]"));
    for (const ErdNode &node : graph.nodes) {
        QString id = node.qualifiedName();
        id.replace(notAllowed, QStringLiteral("_"));
        if (id.isEmpty() || id.front().isDigit())
            id.prepend(QLatin1Char('_'));
        if (taken.contains(id))
            id += QStringLiteral("_%1").arg(node.id);
        taken.insert(id);
        ids.insert(node.id, id);
    }

    QString mermaid = QStringLiteral("erDiagram\n");
    for (const ErdNode &node : graph.nodes) {
        mermaid += QStringLiteral("    %1[%2] {\n")
                       .arg(ids.value(node.id), mermaidString(node.qualifiedName()));
        for (const ErdColumn &column : node.columns) {
            const QString type = mermaidWord(column.type);
            const QString name = mermaidWord(column.name);
            QStringList keys;
            if (column.primaryKey)
                keys << QStringLiteral("PK");
            if (column.foreignKey)
                keys << QStringLiteral("FK");
            mermaid += QStringLiteral("        %1 %2").arg(type, name);
            if (!keys.isEmpty())
                mermaid += QLatin1Char(' ') + keys.join(QStringLiteral(", "));
            // What had to be respelled is kept, as the comment.
            if (type != column.type || name != column.name) {
                mermaid += QLatin1Char(' ')
                    + mermaidString(column.name + QLatin1Char(' ') + column.type);
            }
            mermaid += QLatin1Char('\n');
        }
        mermaid += QStringLiteral("    }\n");
    }
    for (const ErdEdge &edge : graph.edges) {
        if (!ids.contains(edge.from) || !ids.contains(edge.to))
            continue;
        mermaid += QStringLiteral("    %1 %2--o{ %3 : %4\n")
                       .arg(ids.value(edge.to),
                            edge.mandatory ? QStringLiteral("||") : QStringLiteral("|o"),
                            ids.value(edge.from), mermaidString(edge.name));
    }
    return mermaid;
}

} // namespace sqliteman::catalog
