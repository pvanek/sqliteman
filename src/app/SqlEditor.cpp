// SPDX-FileCopyrightText: 2026 Petr Vanek
// SPDX-License-Identifier: GPL-3.0-or-later

#include "SqlEditor.h"

#include "CompletionPopup.h"
#include "Settings.h"
#include "SqlLexer.h"
#include "sql/Comments.h"
#include "sql/Splitter.h"

#include <QContextMenuEvent>
#include <QEvent>
#include <QGuiApplication>
#include <QKeyEvent>
#include <QMenu>
#include <QPalette>
#include <QToolTip>
#include <QtConcurrent/QtConcurrentRun>

#include <algorithm>

namespace sqliteman {

using sql::StatementSpan;

SqlEditor::SqlEditor(QWidget *parent)
    : QsciScintilla(parent), m_lexer(new SqlLexer(this)), m_popup(new CompletionPopup(this))
{
    setUtf8(true);
    setLexer(m_lexer);

    // Line numbers.
    setMarginType(0, NumberMargin);
    setMarginLineNumbers(0, true);
    setMarginWidth(1, 0);
    connect(this, &QsciScintilla::linesChanged, this, &SqlEditor::updateMarginWidth);

    setFolding(BoxedTreeFoldStyle, 2);
    // Scintilla puts the text right against the fold margin; give it air.
    send(SCI_SETMARGINLEFT, 0, 9);
    setIndentationsUseTabs(false);
    setTabWidth(4);
    setAutoIndent(true);
    setBraceMatching(SloppyBraceMatch);
    SendScintilla(SCI_SETMULTIPLESELECTION, 1);
    SendScintilla(SCI_SETADDITIONALSELECTIONTYPING, 1);

    // The statement Run would execute: a faint box.
    SendScintilla(SCI_INDICSETSTYLE, StatementIndicator, INDIC_FULLBOX);
    SendScintilla(SCI_INDICSETUNDER, StatementIndicator, 1);
    SendScintilla(SCI_INDICSETSTYLE, ErrorIndicator, INDIC_SQUIGGLEPIXMAP);
    SendScintilla(SCI_INDICSETFORE, ErrorIndicator, QColor(220, 40, 40));

    m_statementTimer.setSingleShot(true);
    m_statementTimer.setInterval(100);
    connect(&m_statementTimer, &QTimer::timeout, this, &SqlEditor::updateStatementMark);
    connect(this, &QsciScintilla::cursorPositionChanged, &m_statementTimer,
            qOverload<>(&QTimer::start));
    connect(this, &QsciScintilla::textChanged, this, [this] {
        ++m_revision;
        clearErrors();
        m_statementTimer.start();
    });

    // What an error mark says, on hovering over it.
    send(SCI_SETMOUSEDWELLTIME, 500);
    connect(this, &QsciScintillaBase::SCN_DWELLSTART, this, [this](int position, int x, int y) {
        const QString text = position >= 0 ? errorAt(position) : QString();
        if (!text.isEmpty())
            QToolTip::showText(viewport()->mapToGlobal(QPoint(x, y)), text, viewport());
    });
    connect(this, &QsciScintillaBase::SCN_DWELLEND, this, [] { QToolTip::hideText(); });

    m_completionTimer.setSingleShot(true);
    connect(&m_completionTimer, &QTimer::timeout, this, [this] { complete(false); });
    connect(&m_completionWatcher, &QFutureWatcher<catalog::Completion>::finished, this,
            &SqlEditor::showCompletion);
    connect(m_popup, &CompletionPopup::accepted, this, &SqlEditor::applyCompletion);

    connect(&Settings::instance(), &Settings::preferencesChanged, this,
            &SqlEditor::applyPreferences);
    applyPreferences();
}

void SqlEditor::applyPreferences()
{
    const config::Preferences &p = Settings::instance().preferences();
    const QFont font = Settings::instance().editorFont();
    m_lexer->setBaseFont(font);
    setFont(font);
    setMarginsFont(font);
    m_caretLine = p.highlightCurrentLine;
    if (p.widthMark > 0) {
        setEdgeMode(EdgeLine);
        setEdgeColumn(p.widthMark);
    } else {
        setEdgeMode(EdgeNone);
    }
    m_autoCompleteChars = p.completion ? std::max(1, p.completionChars) : 0;
    m_snippets = p.useSnippets ? p.snippets : QMap<QString, QString>();
    applyTheme();
    updateMarginWidth();
}

std::pair<qsizetype, qsizetype> SqlEditor::statementBounds(const QByteArray &text, qsizetype pos)
{
    qsizetype start = 0;
    qsizetype end = text.size();
    for (const StatementSpan &s : sql::splitStatements(text)) {
        qsizetype after = s.offset + s.length; // Past its semicolon, if it has one.
        if (s.kind == StatementSpan::Kind::Sql && s.terminated)
            after = text.indexOf(';', after) + 1;
        if (s.terminated && after <= pos) {
            start = after;
        } else if (s.offset > pos) {
            end = s.offset;
            break;
        }
    }
    return {start, end};
}

void SqlEditor::complete(bool explicitRequest)
{
    const QByteArray text = utf8Text();
    const qsizetype cursor = cursorPosition();
    const auto [start, end] = statementBounds(text, cursor);
    m_statementStart = start;
    m_requestRevision = m_revision;
    m_requestCursor = cursor;
    m_explicitRequest = explicitRequest;

    const QByteArray statement = text.sliced(start, end - start);
    const catalog::SnapshotPtr snapshot = m_snapshot ? m_snapshot() : nullptr;
    m_completionWatcher.setFuture(QtConcurrent::run([statement, cursor = cursor - start, snapshot] {
        static const catalog::Snapshot empty;
        return catalog::complete(statement, cursor, snapshot ? *snapshot : empty);
    }));
}

void SqlEditor::showCompletion()
{
    const catalog::Completion c = m_completionWatcher.result();
    // Typed on, or moved away, meanwhile: a newer request will follow, if any.
    if (m_requestRevision != m_revision || m_requestCursor != cursorPosition())
        return;

    bool show = !c.items.empty() && c.context != catalog::Completion::Context::None;
    if (show && !m_explicitRequest) {
        // Unasked, only once enough of a word is there to narrow the list
        // down (or after a dot), and not when the word is already complete.
        const QByteArray text = utf8Text();
        const bool afterDot = m_requestCursor > 0 && text[m_requestCursor - 1] == '.';
        // A word that is already written needs no list, unless what it
        // matches is a snippet: typing its abbreviation in full is how one
        // asks for it.
        const bool written = c.items.size() == 1
            && c.items.front().label.compare(c.prefix, Qt::CaseInsensitive) == 0
            && c.items.front().kind != catalog::CompletionItem::Kind::Snippet;
        show = m_autoCompleteChars > 0 && (c.prefix.size() >= m_autoCompleteChars || afterDot)
            && !written;
    }
    if (!show) {
        m_popup->hide();
        return;
    }

    const qsizetype at = m_statementStart + c.replaceFrom;
    const int line = int(send(SCI_LINEFROMPOSITION, at));
    const int x = int(send(SCI_POINTXFROMPOSITION, 0, at));
    const int y = int(send(SCI_POINTYFROMPOSITION, 0, at)) + int(send(SCI_TEXTHEIGHT, line));
    m_completion = c;
    m_popup->showItems(c.items, c.prefix, viewport()->mapToGlobal(QPoint(x, y)));
}

void SqlEditor::applyCompletion(const catalog::CompletionItem &item)
{
    const qsizetype from = m_statementStart + m_completion.replaceFrom;
    const qsizetype to = m_statementStart + m_completion.replaceTo;
    const QByteArray text = item.insertText.toUtf8();
    replaceRange(from, to, text);
    // A function puts the caret between its parentheses.
    const qsizetype caret
        = item.caret < 0 ? text.size() : item.insertText.left(item.caret).toUtf8().size();
    setCursorPosition(from + caret);
}

bool SqlEditor::expandSnippet()
{
    if (m_snippets.isEmpty())
        return false;
    const qsizetype pos = cursorPosition();
    const auto start = qsizetype(send(SCI_WORDSTARTPOSITION, pos, true));
    if (start >= pos)
        return false;
    const QString word = QString::fromUtf8(utf8Text().sliced(start, pos - start));
    const auto it = m_snippets.constFind(word);
    if (it == m_snippets.cend())
        return false;
    const QByteArray text = it->toUtf8();
    replaceRange(start, pos, text);
    setCursorPosition(start + text.size());
    return true;
}

void SqlEditor::keyPressEvent(QKeyEvent *event)
{
    if (m_popup->handleKey(event))
        return;
    if (event->key() == Qt::Key_Space && event->modifiers() & Qt::ControlModifier) {
        complete(true);
        return;
    }
    // Before Scintilla, whose Ctrl+/ moves to the previous part of a word.
    if (event->key() == Qt::Key_Slash && event->modifiers() & Qt::ControlModifier) {
        toggleComment();
        return;
    }
    if (event->key() == Qt::Key_Tab && event->modifiers() == Qt::NoModifier && !hasSelectedText()
        && expandSnippet())
        return;
    QsciScintilla::keyPressEvent(event);

    const QString typed = event->text();
    const bool word = typed.size() == 1
        && (typed[0].isLetterOrNumber() || typed[0] == QLatin1Char('_')
            || typed[0] == QLatin1Char('.') || typed[0] == QLatin1Char('"'));
    if (word || (event->key() == Qt::Key_Backspace && m_popup->isVisible())) {
        // Quick while the list is up, so it follows the typing.
        m_completionTimer.start(m_popup->isVisible() ? 30 : 150);
    } else if (!typed.isEmpty() || event->key() == Qt::Key_Left || event->key() == Qt::Key_Right
               || event->key() == Qt::Key_Home || event->key() == Qt::Key_End) {
        m_completionTimer.stop();
        m_popup->hide();
    }
}

void SqlEditor::focusOutEvent(QFocusEvent *event)
{
    if (!m_popup->underMouse())
        m_popup->hide();
    QsciScintilla::focusOutEvent(event);
}

QByteArray SqlEditor::utf8Text() const
{
    const auto length = qsizetype(send(SCI_GETLENGTH));
    return QByteArray(static_cast<const char *>(SendScintillaPtrResult(SCI_GETCHARACTERPOINTER)),
                      length);
}

qsizetype SqlEditor::cursorPosition() const
{
    return qsizetype(send(SCI_GETCURRENTPOS));
}

void SqlEditor::setCursorPosition(qsizetype pos)
{
    send(SCI_GOTOPOS, pos);
}

void SqlEditor::selectRange(qsizetype from, qsizetype to)
{
    send(SCI_SETSEL, from, to);
}

std::pair<qsizetype, qsizetype> SqlEditor::selectionRange() const
{
    return {qsizetype(send(SCI_GETSELECTIONSTART)), qsizetype(send(SCI_GETSELECTIONEND))};
}

void SqlEditor::replaceRange(qsizetype from, qsizetype to, const QByteArray &text)
{
    beginUndoAction();
    send(SCI_SETTARGETSTART, from);
    send(SCI_SETTARGETEND, to);
    // uintptr_t, the one overload taking text: with unsigned long, Win64
    // finds the QColor one, which a const char * converts to, as good a match.
    SendScintilla(SCI_REPLACETARGET, static_cast<uintptr_t>(text.size()), text.constData());
    endUndoAction();
}

void SqlEditor::setSearchMatches(const std::vector<std::pair<qsizetype, qsizetype>> &ranges)
{
    send(SCI_SETINDICATORCURRENT, SearchIndicator);
    send(SCI_INDICATORCLEARRANGE, 0, send(SCI_GETLENGTH));
    for (const auto &[from, to] : ranges) {
        // An empty match, like ^, still shows: as its next character.
        const qsizetype end = from == to ? send(SCI_POSITIONAFTER, from) : to;
        send(SCI_INDICATORFILLRANGE, from, end - from);
    }
}

void SqlEditor::setSearchScope(std::optional<std::pair<qsizetype, qsizetype>> range)
{
    send(SCI_SETINDICATORCURRENT, SearchScopeIndicator);
    send(SCI_INDICATORCLEARRANGE, 0, send(SCI_GETLENGTH));
    if (range && range->second > range->first)
        send(SCI_INDICATORFILLRANGE, range->first, range->second - range->first);
}

std::optional<std::pair<qsizetype, qsizetype>> SqlEditor::searchScope() const
{
    const auto length = qsizetype(send(SCI_GETLENGTH));
    qsizetype from = 0;
    if (send(SCI_INDICATORVALUEAT, SearchScopeIndicator, 0) == 0)
        from = send(SCI_INDICATOREND, SearchScopeIndicator, 0);
    if (from >= length || send(SCI_INDICATORVALUEAT, SearchScopeIndicator, from) == 0)
        return std::nullopt; // None, or all of it deleted.
    return std::pair(from, qsizetype(send(SCI_INDICATOREND, SearchScopeIndicator, from)));
}

void SqlEditor::toggleComment()
{
    const qsizetype anchor = send(SCI_GETANCHOR);
    const qsizetype caret = cursorPosition();
    const qsizetype from = std::min(anchor, caret);
    const qsizetype to = std::max(anchor, caret);
    const auto first = qsizetype(send(SCI_LINEFROMPOSITION, from));
    auto last = qsizetype(send(SCI_LINEFROMPOSITION, to));
    // Whole lines selected end at the start of the next one, which is not
    // meant to be commented with them.
    if (last > first && send(SCI_POSITIONFROMLINE, last) == to)
        --last;
    const qsizetype start = send(SCI_POSITIONFROMLINE, first);
    const qsizetype end = send(SCI_GETLINEENDPOSITION, last);
    const QByteArray lines = utf8Text().sliced(start, end - start);
    const QByteArray toggled = sql::toggleLineComments(lines);
    if (toggled == lines)
        return;

    replaceRange(start, end, toggled);
    // The caret stays where it was in the text, a selection takes in the
    // lines as they are now.
    if (anchor == caret) {
        setCursorPosition(std::max(start, caret + toggled.size() - lines.size()));
    } else if (anchor < caret) {
        selectRange(start, start + toggled.size());
    } else {
        selectRange(start + toggled.size(), start);
    }
}

QMenu *SqlEditor::createContextMenu()
{
    QMenu *menu = createStandardContextMenu();
    menu->addSeparator();
    QAction *comment = menu->addAction(tr("Toggle &Comment"), this, &SqlEditor::toggleComment);
    // Only shown: the key itself is handled in keyPressEvent().
    comment->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_Slash));
    comment->setEnabled(!isReadOnly());
    if (!m_contextActions.isEmpty()) {
        menu->addSeparator();
        menu->addActions(m_contextActions);
    }
    return menu;
}

void SqlEditor::contextMenuEvent(QContextMenuEvent *event)
{
    QMenu *menu = createContextMenu();
    menu->setAttribute(Qt::WA_DeleteOnClose);
    menu->popup(event->globalPos());
}

std::vector<ScriptPiece> SqlEditor::pieces(qsizetype from, qsizetype to) const
{
    const QByteArray text = utf8Text().sliced(from, to - from);
    std::vector<ScriptPiece> out;
    for (const StatementSpan &s : sql::splitStatements(text)) {
        ScriptPiece piece;
        piece.sql = text.mid(s.offset, s.length);
        piece.offset = from + s.offset;
        piece.dotCommand = s.kind == StatementSpan::Kind::DotCommand;
        out.push_back(std::move(piece));
    }
    return out;
}

std::vector<ScriptPiece> SqlEditor::piecesToRun() const
{
    const qsizetype selectionStart = qsizetype(send(SCI_GETSELECTIONSTART));
    const qsizetype selectionEnd = qsizetype(send(SCI_GETSELECTIONEND));
    if (selectionEnd > selectionStart)
        return pieces(selectionStart, selectionEnd);

    const QByteArray text = utf8Text();
    const auto spans = sql::splitStatements(text);
    const int i = sql::statementAt(text, spans, cursorPosition());
    if (i < 0)
        return {};
    const StatementSpan &s = spans[std::size_t(i)];
    ScriptPiece piece;
    piece.sql = text.mid(s.offset, s.length);
    piece.offset = s.offset;
    piece.dotCommand = s.kind == StatementSpan::Kind::DotCommand;
    return {piece};
}

std::vector<ScriptPiece> SqlEditor::allPieces(bool fromCursor) const
{
    const QByteArray text = utf8Text();
    qsizetype from = 0;
    if (fromCursor) {
        const auto spans = sql::splitStatements(text);
        const int i = sql::statementAt(text, spans, cursorPosition());
        if (i >= 0)
            from = spans[std::size_t(i)].offset;
        else
            from = cursorPosition();
    }
    return pieces(from, text.size());
}

std::optional<std::pair<qsizetype, qsizetype>> SqlEditor::currentStatementRange() const
{
    const QByteArray text = utf8Text();
    const auto spans = sql::splitStatements(text);
    const int i = sql::statementAt(text, spans, cursorPosition());
    if (i < 0)
        return std::nullopt;
    const StatementSpan &s = spans[std::size_t(i)];
    return std::pair {s.offset, s.offset + s.length};
}

void SqlEditor::updateStatementMark()
{
    const long length = send(SCI_GETLENGTH);
    send(SCI_SETINDICATORCURRENT, StatementIndicator);
    send(SCI_INDICATORCLEARRANGE, 0, length);
    const bool selection = send(SCI_GETSELECTIONEND) > send(SCI_GETSELECTIONSTART);
    if (selection)
        return; // The selection itself shows what runs.
    if (const auto range = currentStatementRange())
        send(SCI_INDICATORFILLRANGE, range->first, range->second - range->first);
}

void SqlEditor::markError(qsizetype pos, const QString &message)
{
    const long length = send(SCI_GETLENGTH);
    pos = std::clamp<qsizetype>(pos, 0, length);
    qsizetype end = qsizetype(send(SCI_WORDENDPOSITION, pos, true));
    if (end <= pos)
        end = std::min<qsizetype>(pos + 1, length);
    if (end <= pos && pos > 0) // At the very end: mark the last character.
        --pos;
    send(SCI_SETINDICATORCURRENT, ErrorIndicator);
    send(SCI_INDICATORFILLRANGE, pos, std::max<qsizetype>(end - pos, 1));
    m_errors.push_back({{pos, std::max(end, pos + 1)}, message});
}

void SqlEditor::clearErrors()
{
    m_errors.clear();
    send(SCI_SETINDICATORCURRENT, ErrorIndicator);
    send(SCI_INDICATORCLEARRANGE, 0, send(SCI_GETLENGTH));
}

bool SqlEditor::hasErrorAt(qsizetype pos) const
{
    return send(SCI_INDICATORVALUEAT, ErrorIndicator, pos) != 0;
}

QString SqlEditor::errorAt(qsizetype pos) const
{
    for (const auto &[range, message] : m_errors) {
        if (pos >= range.first && pos < range.second && !message.isEmpty())
            return message;
    }
    return {};
}

void SqlEditor::setupSearchIndicators()
{
    const bool dark = QGuiApplication::palette().color(QPalette::Base).lightness() < 128;
    // Under the text, as a marker pen would be: amber, like most editors' matches.
    send(SCI_INDICSETSTYLE, SearchIndicator, INDIC_FULLBOX);
    send(SCI_INDICSETUNDER, SearchIndicator, 1);
    SendScintilla(SCI_INDICSETFORE, static_cast<unsigned long>(SearchIndicator),
                  QColor::fromString(QLatin1String(dark ? "#b8860b" : "#ffc107")));
    send(SCI_INDICSETALPHA, SearchIndicator, dark ? 110 : 120);
    send(SCI_INDICSETOUTLINEALPHA, SearchIndicator, dark ? 200 : 220);
    // Faint, so the matches in it stand out.
    send(SCI_INDICSETSTYLE, SearchScopeIndicator, INDIC_FULLBOX);
    send(SCI_INDICSETUNDER, SearchScopeIndicator, 1);
    SendScintilla(SCI_INDICSETFORE, static_cast<unsigned long>(SearchScopeIndicator),
                  QColor::fromString(QLatin1String(dark ? "#5b8fd6" : "#3d7cc9")));
    send(SCI_INDICSETALPHA, SearchScopeIndicator, dark ? 45 : 30);
    send(SCI_INDICSETOUTLINEALPHA, SearchScopeIndicator, 0);
}

void SqlEditor::changeEvent(QEvent *event)
{
    QsciScintilla::changeEvent(event);
    if (event->type() == QEvent::PaletteChange || event->type() == QEvent::ApplicationPaletteChange)
        applyTheme();
}

void SqlEditor::applyTheme()
{
    // The syntax colors come from the lexer's defaults, so it goes first.
    m_lexer->refreshPalette();
    setLexer(m_lexer);
    setupSearchIndicators();

    const QPalette colors = palette();
    const QColor base = colors.color(QPalette::Base);
    const QColor text = colors.color(QPalette::Text);
    const bool dark = base.lightness() < 128;

    SendScintilla(SCI_INDICSETFORE, StatementIndicator,
                  dark ? QColor(120, 160, 255) : QColor(60, 110, 220));
    SendScintilla(SCI_INDICSETALPHA, StatementIndicator, dark ? 22 : 14);
    setCaretLineVisible(m_caretLine);
    setCaretLineBackgroundColor(dark ? QColor(255, 255, 255, 18) : QColor(0, 0, 0, 10));
    setCaretForegroundColor(text);
    setMatchedBraceBackgroundColor(dark ? QColor(80, 80, 40) : QColor(255, 240, 160));
    setEdgeColor(dark ? QColor(90, 90, 90) : QColor(210, 210, 210));
    setMarginsFont(font());

    applyMarginColors(this);
}

void SqlEditor::applyMarginColors(QsciScintilla *editor)
{
    const QPalette colors = editor->palette();
    const QColor base = colors.color(QPalette::Base);
    auto mix = [](const QColor &a, const QColor &b, float part) {
        return QColor::fromRgbF(a.redF() * (1 - part) + b.redF() * part,
                                a.greenF() * (1 - part) + b.greenF() * part,
                                a.blueF() * (1 - part) + b.blueF() * part);
    };
    // The margins: the window color over the editor's own, numbers muted.
    const QColor marginBack = mix(base, colors.color(QPalette::Window), 0.65f);
    const QColor marginText = mix(base, colors.color(QPalette::Text), 0.55f);
    editor->setMarginsBackgroundColor(marginBack);
    editor->setMarginsForegroundColor(marginText);
    // Without this the fold margin keeps Scintilla's white-and-grey pattern.
    editor->setFoldMarginColors(marginBack, marginBack);
    for (int marker = int(SC_MARKNUM_FOLDEREND); marker <= int(SC_MARKNUM_FOLDEROPEN); ++marker) {
        // The boxes and lines are the marker's background, their plus and
        // minus signs its foreground.
        editor->SendScintilla(SCI_MARKERSETBACK, marker, marginText);
        editor->SendScintilla(SCI_MARKERSETFORE, marker, marginBack);
    }
}

void SqlEditor::updateMarginWidth()
{
    const QString digits = QString::number(std::max(lines(), 100)) + QLatin1Char('0');
    setMarginWidth(0, digits);
}

} // namespace sqliteman
