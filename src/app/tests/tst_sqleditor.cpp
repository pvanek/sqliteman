// SPDX-FileCopyrightText: 2026 Petr Vanek
// SPDX-License-Identifier: GPL-3.0-or-later

#include "CompletionPopup.h"
#include "Settings.h"
#include "SqlEditor.h"
#include "SqlLexer.h"

#include <QTest>

using namespace sqliteman;

class TestSqlEditor : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void runsTheStatementUnderTheCursor()
    {
        SqlEditor editor;
        editor.setText(QStringLiteral("SELECT 1;\nSELECT 2;\n.tables\nSELECT 3"));
        editor.setCursorPosition(qsizetype(12));
        auto pieces = editor.piecesToRun();
        QCOMPARE(pieces.size(), 1u);
        QCOMPARE(pieces[0].sql, QByteArray("SELECT 2"));
        QCOMPARE(pieces[0].offset, 10);

        // A selection runs what it holds.
        editor.selectRange(0, 19);
        pieces = editor.piecesToRun();
        QCOMPARE(pieces.size(), 2u);

        // The whole script, the shell's command marked to skip.
        pieces = editor.allPieces();
        QCOMPARE(pieces.size(), 4u);
        QVERIFY(pieces[2].dotCommand);
        editor.setCursorPosition(qsizetype(12));
        QCOMPARE(editor.allPieces(true).size(), 3u);
    }

    void togglesComments()
    {
        SqlEditor editor;
        editor.setText(QStringLiteral("SELECT 1\nFROM t"));
        editor.selectRange(0, 15);
        editor.toggleComment();
        QCOMPARE(editor.text(), QStringLiteral("-- SELECT 1\n-- FROM t"));
        editor.toggleComment();
        QCOMPARE(editor.text(), QStringLiteral("SELECT 1\nFROM t"));
    }

    void expandsTemplatesWithTab()
    {
        config::Preferences p;
        p.snippets = {{QStringLiteral("sel"), QStringLiteral("SELECT * FROM ")}};
        Settings::instance().setPreferences(p);
        SqlEditor editor;
        editor.show();
        editor.setText(QStringLiteral("sel"));
        editor.setCursorPosition(qsizetype(3));
        QTest::keyClick(&editor, Qt::Key_Tab);
        QCOMPARE(editor.text(), QStringLiteral("SELECT * FROM "));
        QCOMPARE(editor.cursorPosition(), 14);
        // A word that is no template: Tab is a tab.
        editor.setText(QStringLiteral("x"));
        editor.setCursorPosition(qsizetype(1));
        QTest::keyClick(&editor, Qt::Key_Tab);
        QVERIFY(editor.text().startsWith(QLatin1String("x "))
                || editor.text().startsWith(QLatin1String("x\t")));
    }

    void marksErrors()
    {
        SqlEditor editor;
        editor.setText(QStringLiteral("SELECT nocolumn FROM t"));
        editor.markError(7, QStringLiteral("no such column: nocolumn"));
        QVERIFY(editor.hasErrorAt(8));
        QCOMPARE(editor.errorAt(8), QStringLiteral("no such column: nocolumn"));
        QVERIFY(!editor.hasErrorAt(1));
        editor.setText(QStringLiteral("SELECT 1")); // Editing clears them.
        QVERIFY(!editor.hasErrorAt(1));
    }

    void completesFromTheSchema()
    {
        auto snapshot = std::make_shared<catalog::Snapshot>();
        catalog::DatabaseInfo main;
        main.name = QStringLiteral("main");
        catalog::SchemaObject t;
        t.schema = QStringLiteral("main");
        t.name = QStringLiteral("customer");
        t.columns.push_back(
            {0, QStringLiteral("city"), QStringLiteral("TEXT"), false, {}, false, 0, 0});
        main.objects.push_back(t);
        snapshot->databases.push_back(main);

        SqlEditor editor;
        editor.show();
        editor.setSnapshotProvider([snapshot] { return snapshot; });
        editor.setText(QStringLiteral("SELECT * FROM cus"));
        editor.setCursorPosition(qsizetype(17));
        editor.complete(true);
        QTRY_VERIFY(editor.completionPopup()->isVisible());
        QTest::keyClick(&editor, Qt::Key_Return);
        QCOMPARE(editor.text(), QStringLiteral("SELECT * FROM customer"));
    }

    // Typed in lower case, a snippet and a keyword go in that way, and the
    // snippet leaves the caret where the table's name goes.
    void completesInTheCaseTyped()
    {
        auto snapshot = std::make_shared<catalog::Snapshot>();
        catalog::DatabaseInfo main;
        main.name = QStringLiteral("main");
        snapshot->databases.push_back(main);

        SqlEditor editor;
        editor.show();
        editor.setSnapshotProvider([snapshot] { return snapshot; });
        editor.setText(QStringLiteral("scf"));
        editor.setCursorPosition(qsizetype(3));
        editor.complete(true);
        QTRY_VERIFY(editor.completionPopup()->isVisible());
        QTest::keyClick(&editor, Qt::Key_Return);
        QCOMPARE(editor.text(), QStringLiteral("select count(1) from "));
        QCOMPARE(editor.cursorPosition(), qsizetype(21));

        editor.setText(QStringLiteral("sfl"));
        editor.setCursorPosition(qsizetype(3));
        editor.complete(true);
        QTRY_VERIFY(editor.completionPopup()->isVisible());
        QTest::keyClick(&editor, Qt::Key_Return);
        QCOMPARE(editor.text(), QStringLiteral("select * from  limit 100"));
        QCOMPARE(editor.cursorPosition(), qsizetype(14));

        editor.setText(QStringLiteral("selec"));
        editor.setCursorPosition(qsizetype(5));
        editor.complete(true);
        QTRY_VERIFY(editor.completionPopup()->isVisible());
        QTest::keyClick(&editor, Qt::Key_Return);
        QCOMPARE(editor.text(), QStringLiteral("select"));
    }

    void lexerStateFitsInALine()
    {
        sql::LexState state;
        state.mode = sql::LexState::Mode::QuotedIdentifier;
        state.close = ']';
        state.atStatementStart = false;
        QCOMPARE(SqlLexer::decode(SqlLexer::encode(state)), state);
        state = {};
        state.mode = sql::LexState::Mode::String;
        state.blob = true;
        QCOMPARE(SqlLexer::decode(SqlLexer::encode(state)), state);
    }

    void foldsTriggerBodies()
    {
        const QByteArray text = "CREATE TRIGGER t AFTER INSERT ON x BEGIN";
        const int depth = SqlLexer::foldDepthAfter(sql::tokenize(text), text, 0);
        QCOMPARE(depth, 2); // The statement, and the body in it.
        const QByteArray end = "END;";
        QCOMPARE(SqlLexer::foldDepthAfter(sql::tokenize(end), end, depth), 0);
        const QByteArray begin = "BEGIN;"; // A transaction folds nothing.
        QCOMPARE(SqlLexer::foldDepthAfter(sql::tokenize(begin), begin, 0), 0);
    }
};

QTEST_MAIN(TestSqlEditor)
#include "tst_sqleditor.moc"
