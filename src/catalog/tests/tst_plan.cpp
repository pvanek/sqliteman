// SPDX-FileCopyrightText: 2026 Petr Vanek
// SPDX-License-Identifier: GPL-3.0-or-later

#include "TestDatabase.h"

#include "catalog/Plan.h"

#include <QTest>

using namespace sqliteman::catalog;

class TestPlan : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void readsTheTree()
    {
        TestDatabase db;
        db.exec("CREATE TABLE t (a INTEGER, b TEXT); CREATE INDEX t_a ON t (a);"
                "CREATE TABLE u (x INTEGER)");
        const auto rows = db.rows(
            explainStatement("SELECT * FROM t WHERE a = 1 AND b IN (SELECT x FROM u) ORDER BY b"));
        const auto plan = parsePlan(rows);
        QVERIFY(plan.has_value());
        QVERIFY(!plan->isEmpty());

        // The search through t_a, the subquery's scan of u under its own
        // step, and the sort.
        bool search = false, scan = false, sort = false, nested = false;
        for (const PlanStep &step : plan->steps) {
            if (step.kind == PlanStep::Kind::Search && step.object == QLatin1String("t_a"))
                search = true;
            if (step.kind == PlanStep::Kind::Scan && step.object == QLatin1String("u"))
                scan = true;
            if (step.kind == PlanStep::Kind::TempBTree)
                sort = true;
            nested = nested || !step.children.empty();
        }
        QVERIFY(search);
        QVERIFY(scan);
        QVERIFY(sort);
        QVERIFY(nested);
        const QString text = planText(*plan);
        QVERIFY(text.startsWith(QLatin1String("QUERY PLAN\n|--")));
        QVERIFY(text.contains(QLatin1String("\n   |--")));
    }

    void namesTablesRatherThanAliases()
    {
        TestDatabase db;
        db.exec("CREATE TABLE Customer (id INTEGER PRIMARY KEY, city TEXT);"
                "CREATE TABLE \"Order\" (id INTEGER PRIMARY KEY, customer INTEGER);"
                "CREATE INDEX order_customer ON \"Order\" (customer)");
        const QByteArray sql = "SELECT * FROM Customer c JOIN \"Order\" AS o ON o.customer = c.id";
        auto plan = parsePlan(db.rows(explainStatement(sql)));
        QVERIFY(plan.has_value());
        nameTables(*plan, sql);
        QStringList objects;
        for (const PlanStep &step : plan->steps)
            objects << step.object;
        // Customer and Order, by whichever way the planner reads them, and
        // neither by its alias.
        const QString all = objects.join(u',');
        QVERIFY2(objects.contains(QStringLiteral("Customer"))
                     || all.contains(QLatin1String("order_customer")),
                 qPrintable(all));
        QVERIFY2(!objects.contains(QStringLiteral("c")) && !objects.contains(QStringLiteral("o")),
                 qPrintable(all));
    }

    void notAPlan()
    {
        TestDatabase db;
        QVERIFY(!parsePlan(db.rows("SELECT 1, 2")).has_value());
    }
};

QTEST_GUILESS_MAIN(TestPlan)
#include "tst_plan.moc"
