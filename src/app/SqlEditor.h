// SPDX-FileCopyrightText: 2026 Petr Vanek
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "Qsci.h"
#include "catalog/Completion.h"

#include <QFutureWatcher>
#include <QMap>
#include <QTimer>

#include <functional>
#include <optional>
#include <vector>

namespace sqliteman {

class CompletionPopup;
class SqlLexer;

// A piece of the script to run: one statement, or a shell command to skip.
struct ScriptPiece
{
    QByteArray sql;
    qsizetype offset = 0; // Of sql in the document, in bytes.
    bool dotCommand = false;
};

// The SQL text editor: QScintilla with SQLite highlighting, line numbers,
// and a mark on the statement Run would execute. All positions are UTF-8
// byte offsets, as in Scintilla.
class SqlEditor : public QsciScintilla
{
    Q_OBJECT

public:
    explicit SqlEditor(QWidget *parent = nullptr);

    QByteArray utf8Text() const;
    qsizetype cursorPosition() const;
    // In bytes; QScintilla's line and index version stays usable beside it.
    using QsciScintilla::setCursorPosition;
    void setCursorPosition(qsizetype pos);
    void selectRange(qsizetype from, qsizetype to);
    // The main selection in bytes, its start first.
    std::pair<qsizetype, qsizetype> selectionRange() const;
    // Replaces bytes [from, to) with text, as one step to undo.
    void replaceRange(qsizetype from, qsizetype to, const QByteArray &text);
    // Comments the current line, or the selected ones, out with "--", or
    // back in (Ctrl+/).
    void toggleComment();
    // QScintilla's menu, with the editor's own actions added and then the
    // ones set here, such as the page's Run and Explain.
    QMenu *createContextMenu();
    void setContextActions(const QList<QAction *> &actions) { m_contextActions = actions; }

    // What Run executes: the selected statements, or the current one.
    std::vector<ScriptPiece> piecesToRun() const;
    // Every statement of the script, for Run Script, from the one the
    // cursor is in on when fromCursor.
    std::vector<ScriptPiece> allPieces(bool fromCursor = false) const;
    // The current statement's range in bytes, if the cursor is in one.
    std::optional<std::pair<qsizetype, qsizetype>> currentStatementRange() const;

    // Underlines the token at a byte position as an error, until the text
    // changes or clearErrors() is called.
    void markError(qsizetype pos, const QString &message = {});
    void clearErrors();
    bool hasErrorAt(qsizetype pos) const;
    QString errorAt(qsizetype pos) const;

    // Where completion gets catalog data from; it may return null while the
    // data is still loading, and then only keywords are offered.
    using SnapshotProvider = std::function<catalog::SnapshotPtr()>;
    void setSnapshotProvider(SnapshotProvider provider) { m_snapshot = std::move(provider); }
    // Suggests completions for the word at the cursor, off the UI thread.
    // Explicit requests (Ctrl+Space) show suggestions for an empty word too.
    void complete(bool explicitRequest = true);
    CompletionPopup *completionPopup() const { return m_popup; }
    // The statement around a byte position, as completion sees it: the text
    // between the previous and the next statement, even if unterminated.
    static std::pair<qsizetype, qsizetype> statementBounds(const QByteArray &text, qsizetype pos);
    // Margin colours from the editor's palette, for any Scintilla with line
    // numbers or folding. After setLexer(), which resets them to white.
    static void applyMarginColors(QsciScintilla *editor);

    // Find and replace: every match highlighted, and the part of the text a
    // search keeps to, which follows edits as the text moves around it.
    void setSearchMatches(const std::vector<std::pair<qsizetype, qsizetype>> &ranges);
    void setSearchScope(std::optional<std::pair<qsizetype, qsizetype>> range);
    std::optional<std::pair<qsizetype, qsizetype>> searchScope() const;

    // Templates: the word before the cursor and Tab put its text in. Empty
    // turns them off.
    void setSnippets(const QMap<QString, QString> &snippets) { m_snippets = snippets; }
    // Puts in the template for the word before the cursor; false if there is none.
    bool expandSnippet();
    // Letters to type before the list appears by itself; 0 for never.
    void setAutoCompleteChars(int chars) { m_autoCompleteChars = chars; }
    // The preferences' font, current line and width mark, again.
    void applyPreferences();

    static constexpr int StatementIndicator = 8;
    static constexpr int ErrorIndicator = 9;
    static constexpr int SearchIndicator = 21;
    static constexpr int SearchScopeIndicator = 22;

protected:
    void keyPressEvent(QKeyEvent *event) override;
    void focusOutEvent(QFocusEvent *event) override;
    void changeEvent(QEvent *event) override;
    void contextMenuEvent(QContextMenuEvent *event) override;

private:
    void showCompletion();
    void applyCompletion(const catalog::CompletionItem &item);
    void setupSearchIndicators();

    // SendScintilla() with byte offsets, without its overload ambiguities.
    long send(unsigned int message, qsizetype wParam = 0, qsizetype lParam = 0) const
    {
        return SendScintilla(message, static_cast<unsigned long>(wParam),
                             static_cast<long>(lParam));
    }
    void updateStatementMark();
    void updateMarginWidth();
    // Margin, fold and caret colors from the current palette.
    void applyTheme();
    std::vector<ScriptPiece> pieces(qsizetype from, qsizetype to) const;

    SqlLexer *m_lexer = nullptr;
    QList<QAction *> m_contextActions;
    QTimer m_statementTimer;
    QMap<QString, QString> m_snippets;
    int m_autoCompleteChars = 3;
    bool m_caretLine = true;

    SnapshotProvider m_snapshot;
    CompletionPopup *m_popup = nullptr;
    QTimer m_completionTimer;
    QFutureWatcher<catalog::Completion> m_completionWatcher;
    quint64 m_revision = 0; // Counts edits, to drop stale completions.
    quint64 m_requestRevision = 0;
    qsizetype m_requestCursor = 0;
    qsizetype m_statementStart = 0; // Of the statement the completion is for.
    bool m_explicitRequest = false;
    catalog::Completion m_completion; // The one on show.

    std::vector<std::pair<std::pair<qsizetype, qsizetype>, QString>> m_errors; // Range, message.
};

} // namespace sqliteman
