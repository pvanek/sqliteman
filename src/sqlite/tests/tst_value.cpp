// SPDX-FileCopyrightText: 2026 Petr Vanek
// SPDX-License-Identifier: GPL-3.0-or-later

#include "sqlite/Quote.h"
#include "sqlite/Value.h"

#include <QTest>

#include <limits>

using namespace sqliteman::sqlite;

class TestValue : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void realsReadBack()
    {
        QCOMPARE(Value::real(0.1).data(), QByteArray("0.1"));
        QCOMPARE(Value::real(2).data(), QByteArray("2.0"));
        QCOMPARE(Value::real(1e300).toReal(), 1e300);
        QCOMPARE(Value::real(-0.5).data(), QByteArray("-0.5"));
        QVERIFY(Value::real(std::numeric_limits<double>::quiet_NaN()).isNull());
    }

    void userTextBecomesANumberOnlyWhenItReadsAsOne()
    {
        QCOMPARE(Value::fromUserText(QStringLiteral("42")), Value::integer(42));
        QCOMPARE(Value::fromUserText(QStringLiteral("-3")), Value::integer(-3));
        QCOMPARE(Value::fromUserText(QStringLiteral("2.5")).type(), ValueType::Real);
        // Leading zeros, plus signs and spaces are kept as typed.
        QCOMPARE(Value::fromUserText(QStringLiteral("007")).type(), ValueType::Text);
        QCOMPARE(Value::fromUserText(QStringLiteral("+1")).type(), ValueType::Text);
        QCOMPARE(Value::fromUserText(QStringLiteral(" 1")).type(), ValueType::Text);
        QCOMPARE(Value::fromUserText(QString()).type(), ValueType::Text);
        QCOMPARE(Value::fromUserText(QStringLiteral("abc")), Value::text(QStringLiteral("abc")));
    }

    void quoting()
    {
        QCOMPARE(quoteIdentifier(QStringLiteral("My \"Table\"")),
                 QStringLiteral("\"My \"\"Table\"\"\""));
        QCOMPARE(qualifiedName(QStringLiteral("main"), QStringLiteral("t")),
                 QStringLiteral("\"main\".\"t\""));
        QCOMPARE(qualifiedName(QString(), QStringLiteral("t")), QStringLiteral("\"t\""));
        QCOMPARE(quoteLiteral(QStringLiteral("it's")), QStringLiteral("'it''s'"));
        QCOMPARE(blobLiteral(QByteArray("\x0a\xff", 2)), QStringLiteral("X'0AFF'"));
        QCOMPARE(literal(Value()), QStringLiteral("NULL"));
        QCOMPARE(literal(Value::integer(5)), QStringLiteral("5"));
        QCOMPARE(literal(Value::real(1.25)), QStringLiteral("1.25"));
        QCOMPARE(literal(Value::text(QStringLiteral("a'b"))), QStringLiteral("'a''b'"));
        QCOMPARE(literal(Value::blob(QByteArray())), QStringLiteral("X''"));
        QCOMPARE(literal(Value::real(std::numeric_limits<double>::infinity())),
                 QStringLiteral("9e999"));
    }
};

QTEST_GUILESS_MAIN(TestValue)
#include "tst_value.moc"
