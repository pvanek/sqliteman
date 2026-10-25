// SPDX-FileCopyrightText: 2026 Petr Vanek
// SPDX-License-Identifier: GPL-3.0-or-later

#include "TestDatabase.h"

#include "catalog/Pragmas.h"

#include <QTest>

using namespace sqliteman::catalog;

class TestPragmas : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void readsThemAll()
    {
        TestDatabase db;
        const auto outcome = db.run(readPragmasQuery(QStringLiteral("main")));
        QVERIFY2(outcome.ok(), qPrintable(outcome.error));
        // One result each, in the order of the list.
        QCOMPARE(outcome.results.size(), pragmas().size());
        for (std::size_t i = 0; i < pragmas().size(); ++i)
            QVERIFY2(outcome.results[i].rowCount() == 1, qPrintable(pragmas()[i].name));
    }

    void setsThem()
    {
        TestDatabase db;
        const auto set = [&](const char *name, const QString &value) {
            const QByteArray sql
                = setPragmaStatement(QStringLiteral("main"), *pragma(QLatin1String(name)), value);
            QVERIFY2(db.run(sql).ok(), sql.constData());
        };
        set("user_version", QStringLiteral("7"));
        QCOMPARE(db.value("PRAGMA main.user_version"), QStringLiteral("7"));
        set("journal_mode", QStringLiteral("wal"));
        QCOMPARE(db.value("PRAGMA main.journal_mode"), QStringLiteral("wal"));
        set("synchronous", QStringLiteral("1")); // By its number.
        QCOMPARE(db.value("PRAGMA main.synchronous"), QStringLiteral("1"));
        set("recursive_triggers", QStringLiteral("yes"));
        QCOMPARE(db.value("PRAGMA recursive_triggers"), QStringLiteral("1"));
        QVERIFY(setPragmaStatement(QStringLiteral("main"), *pragma(QStringLiteral("page_count")),
                                   QStringLiteral("1"))
                    .isEmpty());
    }

    void statementsAndValues()
    {
        QCOMPARE(setPragmaStatement(QStringLiteral("other db"),
                                    *pragma(QStringLiteral("auto_vacuum")),
                                    QStringLiteral("incremental")),
                 QByteArray("PRAGMA \"other db\".auto_vacuum = incremental"));
        // A pragma of the connection takes no database name.
        QCOMPARE(setPragmaStatement(QStringLiteral("main"), *pragma(QStringLiteral("foreign_keys")),
                                    QStringLiteral("0")),
                 QByteArray("PRAGMA foreign_keys = OFF"));
        QCOMPARE(displayValue(*pragma(QStringLiteral("auto_vacuum")), QStringLiteral("2")),
                 QStringLiteral("INCREMENTAL (2)"));
        QCOMPARE(displayValue(*pragma(QStringLiteral("journal_mode")), QStringLiteral("wal")),
                 QStringLiteral("WAL"));
        QCOMPARE(displayValue(*pragma(QStringLiteral("foreign_keys")), QStringLiteral("1")),
                 QStringLiteral("ON"));
    }
};

QTEST_GUILESS_MAIN(TestPragmas)
#include "tst_pragmas.moc"
