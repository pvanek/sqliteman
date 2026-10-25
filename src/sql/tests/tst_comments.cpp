// SPDX-FileCopyrightText: 2026 Petr Vanek
// SPDX-License-Identifier: GPL-3.0-or-later

#include "sql/Comments.h"

#include <QTest>

using sqliteman::sql::toggleLineComments;

class TestComments : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void toggle_data()
    {
        QTest::addColumn<QByteArray>("lines");
        QTest::addColumn<QByteArray>("toggled");

        QTest::newRow("line") << QByteArray("SELECT 1;") << QByteArray("-- SELECT 1;");
        QTest::newRow("commented") << QByteArray("-- SELECT 1;") << QByteArray("SELECT 1;");
        QTest::newRow("no space") << QByteArray("--SELECT 1;") << QByteArray("SELECT 1;");
        QTest::newRow("indented") << QByteArray("  x") << QByteArray("  -- x");
        QTest::newRow("indented back") << QByteArray("  -- x") << QByteArray("  x");
        // At the least indentation, so the block keeps its shape.
        QTest::newRow("block") << QByteArray("SELECT\n    a\n\nFROM t")
                               << QByteArray("-- SELECT\n--     a\n\n-- FROM t");
        QTest::newRow("block back") << QByteArray("-- SELECT\n--     a\n\n-- FROM t")
                                    << QByteArray("SELECT\n    a\n\nFROM t");
        // One line not commented yet: comment them all, again.
        QTest::newRow("mixed") << QByteArray("-- a\nb") << QByteArray("-- -- a\n-- b");
        QTest::newRow("crlf") << QByteArray("a\r\n\r\nb") << QByteArray("-- a\r\n\r\n-- b");
        QTest::newRow("blank") << QByteArray("  ") << QByteArray("  ");
        QTest::newRow("empty") << QByteArray() << QByteArray();
    }

    void toggle()
    {
        QFETCH(QByteArray, lines);
        QFETCH(QByteArray, toggled);
        QCOMPARE(toggleLineComments(lines), toggled);
    }
};

QTEST_GUILESS_MAIN(TestComments)
#include "tst_comments.moc"
