// SPDX-FileCopyrightText: 2026 Petr Vanek
// SPDX-License-Identifier: GPL-3.0-or-later

#include "TestDatabase.h"

#include "catalog/QueryBuilder.h"

#include <QTest>

using namespace sqliteman::catalog;
using Relation = QuerySpec::Relation;

class TestQueryBuilder : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void everyColumn()
    {
        QuerySpec spec;
        spec.schema = QStringLiteral("main");
        spec.table = QStringLiteral("My Table");
        QCOMPARE(selectStatement(spec), QStringLiteral("SELECT *\nFROM \"My Table\""));
        spec.schema = QStringLiteral("other");
        QCOMPARE(selectStatement(spec), QStringLiteral("SELECT *\nFROM other.\"My Table\""));
    }

    void termsAndOrder()
    {
        QuerySpec spec;
        spec.table = QStringLiteral("t");
        spec.distinct = true;
        spec.columns = {QStringLiteral("a"), QStringLiteral("order")};
        spec.terms = {{QStringLiteral("a"), Relation::Contains, QStringLiteral("50%_off")},
                      {QStringLiteral("b"), Relation::Greater, QStringLiteral("10")},
                      {QStringLiteral("c"), Relation::Equals, QStringLiteral("it's")},
                      {QStringLiteral("d"), Relation::IsNull, QString()}};
        spec.matchAll = false;
        spec.order = {{QStringLiteral("a"), true}};
        spec.limit = 5;
        QCOMPARE(selectStatement(spec),
                 QStringLiteral("SELECT DISTINCT a, \"order\"\n"
                                "FROM t\n"
                                "WHERE a LIKE '%50\\%\\_off%' ESCAPE '\\'\n"
                                "   OR b > 10\n"
                                "   OR c = 'it''s'\n"
                                "   OR d IS NULL\n"
                                "ORDER BY a DESC\n"
                                "LIMIT 5"));
    }

    void runsAndMatchesLiterally()
    {
        TestDatabase db;
        db.exec(
            "CREATE TABLE t (a TEXT, b INT); INSERT INTO t VALUES ('50%_off', 1), ('5000 off', 2),"
            "('half', 3)");
        QuerySpec spec;
        spec.table = QStringLiteral("t");
        spec.columns = {QStringLiteral("b")};
        spec.terms = {{QStringLiteral("a"), Relation::Contains, QStringLiteral("0%_")}};
        QCOMPARE(db.value(selectStatement(spec).toUtf8()), QStringLiteral("1"));
        spec.terms = {{QStringLiteral("a"), Relation::StartsWith, QStringLiteral("5")},
                      {QStringLiteral("b"), Relation::GreaterOrEqual, QStringLiteral("2")}};
        QCOMPARE(db.value(selectStatement(spec).toUtf8()), QStringLiteral("2"));
    }
};

QTEST_GUILESS_MAIN(TestQueryBuilder)
#include "tst_querybuilder.moc"
