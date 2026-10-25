// SPDX-FileCopyrightText: 2026 Petr Vanek
// SPDX-License-Identifier: GPL-3.0-or-later

#include "TestDatabase.h"

#include "catalog/Completion.h"

#include <QTest>

using namespace sqliteman::catalog;
using Context = Completion::Context;
using Kind = CompletionItem::Kind;

namespace {

// Completes where | is.
Completion at(const char *withCursor, const Snapshot &snapshot)
{
    QByteArray statement(withCursor);
    const qsizetype cursor = statement.indexOf('|');
    statement.remove(cursor, 1);
    return complete(statement, cursor, snapshot);
}

QStringList labels(const Completion &c, Kind kind)
{
    QStringList out;
    for (const CompletionItem &i : c.items)
        if (i.kind == kind)
            out << i.label;
    return out;
}

} // namespace

class TestCompletion : public QObject
{
    Q_OBJECT

private:
    SnapshotPtr m_snapshot;
    TestDatabase *m_db = nullptr;

private Q_SLOTS:
    void initTestCase()
    {
        m_db = new TestDatabase;
        m_db->exec("CREATE TABLE customer (id INTEGER PRIMARY KEY, customer_name TEXT, city TEXT);"
                   "CREATE TABLE \"Order Items\" (id INTEGER, qty INT);"
                   "CREATE VIEW cities AS SELECT DISTINCT city FROM customer;");
        m_db->exec(QStringLiteral("ATTACH '%1/archive.db' AS archive").arg(m_db->dir()).toUtf8());
        m_db->exec("CREATE TABLE archive.old_orders (n INT)");
        m_snapshot = m_db->snapshot();
        QVERIFY(m_snapshot);
    }

    void cleanupTestCase() { delete m_db; }

    void tablesAfterFrom()
    {
        const Completion c = at("SELECT * FROM cu|", *m_snapshot);
        QCOMPARE(c.context, Context::Relation);
        QCOMPARE(c.prefix, QStringLiteral("cu"));
        QCOMPARE(c.replaceFrom, 14);
        QCOMPARE(c.items.front().label, QStringLiteral("customer"));
        QVERIFY(labels(at("SELECT * FROM |", *m_snapshot), Kind::View)
                    .contains(QStringLiteral("cities")));
        // An attached database's tables go in qualified.
        const Completion old = at("SELECT * FROM old|", *m_snapshot);
        QCOMPARE(old.items.front().insertText, QStringLiteral("archive.old_orders"));
        // Names that need quotes get them.
        const Completion order = at("INSERT INTO Ord|", *m_snapshot);
        QCOMPARE(order.items.front().insertText, QStringLiteral("\"Order Items\""));
    }

    void columnsOfTheTablesInScope()
    {
        const Completion c = at("SELECT cn| FROM customer c", *m_snapshot);
        QCOMPARE(c.context, Context::Column);
        // cn: the word starts of customer_name.
        QCOMPARE(c.items.front().label, QStringLiteral("customer_name"));
        QCOMPARE(c.items.front().kind, Kind::Column);
        const Completion where = at("SELECT * FROM customer WHERE ci|", *m_snapshot);
        QCOMPARE(where.items.front().label, QStringLiteral("city"));
    }

    void qualifiedByAliasOrDatabase()
    {
        Completion c = at("SELECT c.| FROM customer AS c", *m_snapshot);
        QCOMPARE(c.context, Context::Qualified);
        QCOMPARE(labels(c, Kind::Column),
                 (QStringList {QStringLiteral("city"), QStringLiteral("customer_name"),
                               QStringLiteral("id")}));
        c = at("SELECT * FROM archive.|", *m_snapshot);
        QCOMPARE(labels(c, Kind::Table), QStringList {QStringLiteral("old_orders")});
        c = at("SELECT o.| FROM \"Order Items\" o", *m_snapshot);
        QVERIFY(labels(c, Kind::Column).contains(QStringLiteral("qty")));
    }

    void keywordsAfterACompleteName()
    {
        const Completion c = at("SELECT * FROM customer WH|", *m_snapshot);
        QCOMPARE(c.context, Context::Keyword);
        QCOMPARE(c.items.front().label, QStringLiteral("WHERE"));
    }

    // Keywords come out as they are being typed: in lower case when that is
    // how the word was started, in capitals otherwise.
    void keywordsFollowTheTypedCase()
    {
        const Completion lower = at("sel|", *m_snapshot);
        QVERIFY(labels(lower, Kind::Keyword).contains(QStringLiteral("select")));
        QVERIFY(!labels(lower, Kind::Keyword).contains(QStringLiteral("SELECT")));
        QCOMPARE(at("select * from customer wh|", *m_snapshot).items.front().insertText,
                 QStringLiteral("where"));
        // Capitals, or a mix of both, or nothing typed yet: capitals.
        QVERIFY(labels(at("SEL|", *m_snapshot), Kind::Keyword).contains(QStringLiteral("SELECT")));
        QVERIFY(labels(at("Sel|", *m_snapshot), Kind::Keyword).contains(QStringLiteral("SELECT")));
        QVERIFY(labels(at("SELECT * FROM customer |", *m_snapshot), Kind::Keyword)
                    .contains(QStringLiteral("WHERE")));
        // The common ones still go ahead of the rare ones, in either case.
        QCOMPARE(at("select * from customer whe|", *m_snapshot).items.front().label,
                 QStringLiteral("where"));
    }

    // Short words standing for whole statements.
    void snippets()
    {
        const Completion c = at("scf|", *m_snapshot);
        const CompletionItem &first = c.items.front();
        QCOMPARE(first.kind, Kind::Snippet);
        QCOMPARE(first.label, QStringLiteral("scf"));
        // Typed in lower case, so the keywords come out that way too.
        QCOMPARE(first.insertText, QStringLiteral("select count(1) from "));
        QCOMPARE(first.caret, int(first.insertText.size()));
        QVERIFY(!first.detail.isEmpty());

        // The other way round for an abbreviation typed in capitals.
        QCOMPARE(at("SCF|", *m_snapshot).items.front().insertText,
                 QStringLiteral("SELECT count(1) FROM "));
        QCOMPARE(at("sf|", *m_snapshot).items.front().insertText, QStringLiteral("select * from "));

        // The caret lands where the table's name goes, not at the end.
        const CompletionItem limit = at("sfl|", *m_snapshot).items.front();
        QCOMPARE(limit.insertText, QStringLiteral("select * from  limit 100"));
        QCOMPARE(limit.caret, int(QStringLiteral("select * from ").size()));

        // A snippet is offered beside the keywords it stands for, never
        // instead of them.
        QVERIFY(labels(at("se|", *m_snapshot), Kind::Keyword).contains(QStringLiteral("select")));
    }

    void snippetsStayOutOfTheWay()
    {
        // Nothing typed: the list is about this statement, not a menu of
        // everything that could be written.
        QVERIFY(labels(at("|", *m_snapshot), Kind::Snippet).isEmpty());
        // Where a table belongs, a snippet does not.
        QVERIFY(labels(at("SELECT * FROM |", *m_snapshot), Kind::Snippet).isEmpty());
        QVERIFY(labels(at("SELECT * FROM s|", *m_snapshot), Kind::Snippet).isEmpty());
    }

    void functions()
    {
        const Completion c = at("SELECT json_ext| FROM customer", *m_snapshot);
        QCOMPARE(c.items.front().kind, Kind::Function);
        QCOMPARE(c.items.front().insertText, QStringLiteral("json_extract()"));
        QCOMPARE(c.items.front().caret, 13); // Between the parentheses.
    }

    void pragmas()
    {
        const Completion c = at("PRAGMA journ|", *m_snapshot);
        QCOMPARE(c.context, Context::Pragma);
        QCOMPARE(c.items.front().label, QStringLiteral("journal_mode"));
    }

    void nothingInCommentsOrStrings()
    {
        QCOMPARE(at("SELECT 'cu|' FROM customer", *m_snapshot).context, Context::None);
        QCOMPARE(at("SELECT 1 -- cu|", *m_snapshot).context, Context::None);
        QCOMPARE(at("SELECT /* cu| */ 1", *m_snapshot).context, Context::None);
    }

    void fuzzy()
    {
        QVERIFY(fuzzyScore(QStringLiteral("cus"), QStringLiteral("customer"))
                > fuzzyScore(QStringLiteral("cus"), QStringLiteral("accustomed")));
        QCOMPARE(fuzzyScore(QStringLiteral("xyz"), QStringLiteral("customer")), 0);
        QCOMPARE(fuzzyMatchPositions(QStringLiteral("cn"), QStringLiteral("customer_name")),
                 (std::vector<qsizetype> {0, 9}));
    }
};

QTEST_GUILESS_MAIN(TestCompletion)
#include "tst_completion.moc"
