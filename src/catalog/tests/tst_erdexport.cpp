// SPDX-FileCopyrightText: 2026 Petr Vanek
// SPDX-License-Identifier: GPL-3.0-or-later

#include "catalog/ErdExport.h"

#include <QRegularExpression>
#include <QTest>

using namespace sqliteman::catalog;

namespace {

ErdNode makeNode(unsigned int id, const char *schema, const char *name)
{
    ErdNode node;
    node.id = id;
    node.schema = QString::fromUtf8(schema);
    node.name = QString::fromUtf8(name);
    return node;
}

// author <- book (author_id NOT NULL), book <- book (sequel_of, nullable).
ErdGraph library()
{
    ErdGraph graph;
    graph.focus = 2;
    ErdNode author = makeNode(1, "public", "author");
    author.columns.push_back({QStringLiteral("id"), QStringLiteral("integer"), true});
    author.otherColumns = 3;
    ErdNode book = makeNode(2, "public", "book");
    book.columns.push_back({QStringLiteral("id"), QStringLiteral("integer"), true});
    book.columns.push_back(
        {QStringLiteral("author_id"), QStringLiteral("integer"), false, true, true});
    book.columns.push_back({QStringLiteral("sequel_of"), QStringLiteral("integer"), false, true});
    graph.nodes = {book, author};

    ErdEdge written;
    written.name = QStringLiteral("book_author_id_fkey");
    written.from = 2;
    written.to = 1;
    written.fromColumns = {QStringLiteral("author_id")};
    written.toColumns = {QStringLiteral("id")};
    written.mandatory = true;
    ErdEdge sequel;
    sequel.name = QStringLiteral("book_sequel_of_fkey");
    sequel.from = 2;
    sequel.to = 2;
    sequel.fromColumns = {QStringLiteral("sequel_of")};
    sequel.toColumns = {QStringLiteral("id")};
    graph.edges = {written, sequel};
    return graph;
}

} // namespace

class TestErdExport : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void dotHasEveryTableAndKey()
    {
        const QString dot = erdToDot(library());
        QVERIFY(dot.startsWith(QLatin1String("digraph erd {")));
        QVERIFY(dot.trimmed().endsWith(QLatin1Char('}')));
        QVERIFY(dot.contains(QLatin1String("t1 [")));
        QVERIFY(dot.contains(QLatin1String("t2 [")));
        // From the key's own row to the referenced column's.
        QVERIFY(
            dot.contains(QLatin1String("t2:c1 -> t1:c0 [arrowtail=crowodot, arrowhead=teetee")));
        QVERIFY(
            dot.contains(QLatin1String("t2:c2 -> t2:c0 [arrowtail=crowodot, arrowhead=teeodot")));
        QVERIFY(dot.contains(QLatin1String("<B>id</B>")));
        QVERIFY(dot.contains(QLatin1String("<I>author_id</I>")));
        QVERIFY(dot.contains(QLatin1String("+ 3 columns")));
        // One schema: no frames.
        QVERIFY(!dot.contains(QLatin1String("subgraph")));
        QCOMPARE(dot.count(QLatin1Char('{')), dot.count(QLatin1Char('}')));
    }

    void dotFramesEachSchema()
    {
        ErdGraph graph = library();
        graph.nodes[1].schema = QStringLiteral("people");
        const QString dot = erdToDot(graph);
        QCOMPARE(dot.count(QLatin1String("subgraph cluster_")), 2);
        QVERIFY(dot.contains(QLatin1String("label=\"people\"")));
        QCOMPARE(dot.count(QLatin1Char('{')), dot.count(QLatin1Char('}')));
    }

    void dotEscapesNames()
    {
        ErdGraph graph = library();
        graph.nodes[0].name = QStringLiteral("a<b>&\"c\"");
        graph.edges[0].name = QStringLiteral("say \"hi\"");
        const QString dot = erdToDot(graph);
        QVERIFY(dot.contains(QLatin1String("a&lt;b&gt;&amp;&quot;c&quot;")));
        QVERIFY(dot.contains(QLatin1String("tooltip=\"say \\\"hi\\\"\"")));
    }

    void mermaidHasEveryTableAndKey()
    {
        const QString mermaid = erdToMermaid(library());
        QVERIFY(mermaid.startsWith(QLatin1String("erDiagram\n")));
        QVERIFY(mermaid.contains(QLatin1String("    public_book[\"public.book\"] {\n")));
        QVERIFY(mermaid.contains(QLatin1String("        integer id PK\n")));
        QVERIFY(mermaid.contains(QLatin1String("        integer author_id FK\n")));
        // The referenced table on the left, the one with the key on the right.
        QVERIFY(mermaid.contains(
            QLatin1String("    public_author ||--o{ public_book : \"book_author_id_fkey\"\n")));
        QVERIFY(mermaid.contains(
            QLatin1String("    public_book |o--o{ public_book : \"book_sequel_of_fkey\"\n")));
    }

    void mermaidRespellsWhatItCannotTake()
    {
        ErdGraph graph = library();
        graph.nodes[0].name = QStringLiteral("my book");
        graph.nodes[0].columns.push_back(
            {QStringLiteral("2nd \"col\""), QStringLiteral("numeric(10,2)"), true, true});
        const QString mermaid = erdToMermaid(graph);
        QVERIFY(mermaid.contains(QLatin1String("    public_my_book[\"public.my book\"] {\n")));
        QVERIFY(mermaid.contains(QLatin1String(
            "        numeric(10_2) _2nd__col_ PK, FK \"2nd #quot;col#quot; numeric(10,2)\"\n")));
        QVERIFY(mermaid.contains(QLatin1String("public_author ||--o{ public_my_book")));
    }

    void mermaidKeepsIdsApart()
    {
        ErdGraph graph = library();
        graph.nodes[0].name = QStringLiteral("a-b");
        graph.nodes[1].name = QStringLiteral("a b");
        const QString mermaid = erdToMermaid(graph);
        QVERIFY(mermaid.contains(QLatin1String("    public_a_b[\"public.a-b\"]")));
        QVERIFY(mermaid.contains(QLatin1String("    public_a_b_1[\"public.a b\"]")));
    }

    void emptyGraphsAreStillValid()
    {
        QCOMPARE(erdToMermaid({}), QStringLiteral("erDiagram\n"));
        QVERIFY(erdToDot({}).trimmed().endsWith(QLatin1Char('}')));
    }
};

QTEST_GUILESS_MAIN(TestErdExport)
#include "tst_erdexport.moc"
