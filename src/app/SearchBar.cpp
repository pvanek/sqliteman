// SPDX-FileCopyrightText: 2026 Petr Vanek
// SPDX-License-Identifier: GPL-3.0-or-later

#include "SearchBar.h"

#include "CompletionPopup.h"
#include "SqlEditor.h"
#include "ui_SearchBar.h"

#include <QAction>
#include <QApplication>
#include <QKeyEvent>
#include <QLineEdit>
#include <QSettings>
#include <QSignalBlocker>
#include <QStyle>

namespace sqliteman {

namespace {

constexpr int MaxHistory = 15;

const QString PatternsKey = QStringLiteral("search/patterns");
const QString ReplacementsKey = QStringLiteral("search/replacements");
const QString ModeKey = QStringLiteral("search/mode");
const QString MatchCaseKey = QStringLiteral("search/matchCase");

QColor mix(const QColor &a, const QColor &b, qreal amount)
{
    return QColor::fromRgbF(float(a.redF() * (1 - amount) + b.redF() * amount),
                            float(a.greenF() * (1 - amount) + b.greenF() * amount),
                            float(a.blueF() * (1 - amount) + b.blueF() * amount));
}

bool hasLineBreak(const QByteArray &text)
{
    return text.contains('\n') || text.contains('\r');
}

} // namespace

SearchBar::SearchBar(SqlEditor *editor, QWidget *parent)
    : QWidget(parent), m_ui(std::make_unique<Ui::SearchBar>()), m_editor(editor)
{
    m_ui->setupUi(this);
    m_ui->closeButton->setIcon(QIcon::fromTheme(
        QStringLiteral("window-close"), style()->standardIcon(QStyle::SP_TitleBarCloseButton)));
    // Without an icon theme the button shows its text instead.
    m_ui->powerButton->setIcon(QIcon::fromTheme(QStringLiteral("edit-find-replace")));

    QSettings settings;
    m_ui->findField->addItems(settings.value(PatternsKey).toStringList());
    m_ui->replaceField->addItems(settings.value(ReplacementsKey).toStringList());
    m_ui->mode->setCurrentIndex(
        std::clamp(settings.value(ModeKey).toInt(), 0, m_ui->mode->count() - 1));
    m_ui->matchCase->setChecked(settings.value(MatchCaseKey).toBool());

    auto action = [this](const QString &icon, const QString &text, const QList<QKeySequence> &keys,
                         auto slot) {
        auto *a = new QAction(QIcon::fromTheme(icon), text, this);
        a->setShortcuts(keys);
        a->setShortcutContext(Qt::WidgetWithChildrenShortcut);
        connect(a, &QAction::triggered, this, slot);
        m_actions << a;
        return a;
    };
    action(QStringLiteral("edit-find"), tr("&Find…"), QKeySequence::keyBindings(QKeySequence::Find),
           [this] { toggle(Mode::Incremental); });
    // Kate's keys, rather than the platform's Ctrl+H for replace.
    action(QStringLiteral("edit-find-replace"), tr("Find and &Replace…"),
           {QKeySequence(Qt::CTRL | Qt::Key_R)}, [this] { toggle(Mode::Power); });
    QAction *next = action({}, tr("Find &Next"), QKeySequence::keyBindings(QKeySequence::FindNext),
                           [this] { findNext(); });
    QAction *previous
        = action({}, tr("Find Pre&vious"), QKeySequence::keyBindings(QKeySequence::FindPrevious),
                 [this] { findPrevious(); });
    action({}, tr("Find &Selected"), {QKeySequence(Qt::CTRL | Qt::Key_H)},
           [this] { findSelected(false); });
    action({}, tr("Find Selected &Backwards"), {QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_H)},
           [this] { findSelected(true); });
    auto withKey = [](QWidget *w, const QAction *a) {
        if (!a->shortcut().isEmpty())
            w->setToolTip(QStringLiteral("%1 (%2)").arg(
                w->toolTip(), a->shortcut().toString(QKeySequence::NativeText)));
    };
    withKey(m_ui->nextButton, next);
    withKey(m_ui->previousButton, previous);
    withKey(m_ui->powerButton, m_actions[1]);

    connect(m_ui->findField->lineEdit(), &QLineEdit::textEdited, this, [this] {
        if (m_mode == Mode::Incremental) {
            incrementalSearch();
            return;
        }
        // Searching waits for Enter; until then, only say what is wrong.
        const sql::TextSearch s = search();
        if (!s.pattern().isEmpty() && !s.isValid())
            setState(State::Invalid, tr("Invalid regular expression: %1").arg(s.errorString()));
        else
            setState(State::None);
    });
    connect(m_ui->findField, &QComboBox::textActivated, this, [this] {
        if (m_mode == Mode::Incremental)
            incrementalSearch();
    });
    connect(m_ui->nextButton, &QToolButton::clicked, this, &SearchBar::findNext);
    connect(m_ui->previousButton, &QToolButton::clicked, this, &SearchBar::findPrevious);
    connect(m_ui->findAllButton, &QPushButton::clicked, this, &SearchBar::findAll);
    connect(m_ui->replaceButton, &QPushButton::clicked, this, &SearchBar::replace);
    connect(m_ui->replaceAllButton, &QPushButton::clicked, this, &SearchBar::replaceAll);
    connect(m_ui->closeButton, &QToolButton::clicked, this, &SearchBar::closeBar);
    connect(m_ui->powerButton, &QToolButton::toggled, this, [this](bool power) {
        setMode(power ? Mode::Power : Mode::Incremental);
        m_ui->findField->setFocus();
    });
    connect(m_ui->matchCase, &QCheckBox::toggled, this, [this](bool on) {
        QSettings().setValue(MatchCaseKey, on);
        if (m_mode == Mode::Incremental && isVisible())
            incrementalSearch();
        else if (m_highlighting)
            highlight(search());
    });
    connect(m_ui->mode, &QComboBox::currentIndexChanged, this, [this](int index) {
        QSettings().setValue(ModeKey, index);
        if (m_highlighting)
            highlight(search());
    });
    connect(m_ui->selectionOnly, &QCheckBox::toggled, this, [this](bool on) {
        if (on)
            setScopeFromSelection();
        else
            m_editor->setSearchScope(std::nullopt);
    });

    // Matches on show follow the text as it is edited.
    m_highlightTimer.setSingleShot(true);
    m_highlightTimer.setInterval(150);
    connect(&m_highlightTimer, &QTimer::timeout, this, [this] { highlight(m_highlighted); });
    connect(m_editor, &QsciScintilla::textChanged, this, [this] {
        if (m_highlighting)
            m_highlightTimer.start();
    });

    m_ui->findField->lineEdit()->installEventFilter(this);
    m_ui->replaceField->lineEdit()->installEventFilter(this);
    m_editor->installEventFilter(this);

    setMode(Mode::Incremental);
    setVisible(false);
}

SearchBar::~SearchBar() = default;

QString SearchBar::statusText() const
{
    return m_ui->status->text();
}

QComboBox *SearchBar::findField() const
{
    return m_ui->findField;
}

QComboBox *SearchBar::replaceField() const
{
    return m_ui->replaceField;
}

sql::TextSearch SearchBar::search() const
{
    // Incremental search is for plain text only, as in Kate.
    const auto mode = m_mode == Mode::Incremental
        ? sql::TextSearch::Mode::PlainText
        : sql::TextSearch::Mode(m_ui->mode->currentIndex());
    return sql::TextSearch(m_ui->findField->currentText(), mode, m_ui->matchCase->isChecked());
}

void SearchBar::setMode(Mode mode)
{
    m_mode = mode;
    const bool power = mode == Mode::Power;
    const QList<QWidget *> powerOnly {m_ui->findAllButton, m_ui->mode,
                                      m_ui->replaceLabel,  m_ui->replaceField,
                                      m_ui->replaceButton, m_ui->replaceAllButton,
                                      m_ui->selectionOnly};
    for (QWidget *w : powerOnly)
        w->setVisible(power);
    const QSignalBlocker blocker(m_ui->powerButton);
    m_ui->powerButton->setChecked(power);
    m_ui->replaceButton->setEnabled(!m_editor->isReadOnly());
    m_ui->replaceAllButton->setEnabled(!m_editor->isReadOnly());
    setState(State::None);
}

bool SearchBar::takeSelection(bool forPower)
{
    const auto [from, to] = m_editor->selectionRange();
    if (from == to)
        return false;
    const QByteArray selected = m_editor->utf8Text().sliced(from, to - from);
    if (hasLineBreak(selected)) {
        // Several lines are not something to find, but where to find it.
        if (forPower)
            m_ui->selectionOnly->setChecked(true);
        return false;
    }
    QString text = QString::fromUtf8(selected);
    if (forPower) {
        // Found as it is, whichever way the pattern is read.
        if (m_ui->mode->currentIndex() == int(sql::TextSearch::Mode::RegularExpression))
            text = QRegularExpression::escape(text);
        else if (m_ui->mode->currentIndex() == int(sql::TextSearch::Mode::EscapeSequences))
            text.replace(QLatin1Char('\\'), QStringLiteral("\\\\"));
    }
    m_ui->findField->setEditText(text);
    return true;
}

void SearchBar::openIncremental()
{
    setMode(Mode::Incremental);
    m_start = m_editor->selectionRange().first;
    const bool selected = takeSelection(false);
    show();
    m_ui->findField->setFocus();
    m_ui->findField->lineEdit()->selectAll();
    // The selection is the first match; the last search's text waits for
    // typing, which replaces it.
    if (selected)
        incrementalSearch();
}

void SearchBar::openPower()
{
    m_ui->selectionOnly->setChecked(false);
    setMode(Mode::Power);
    takeSelection(true);
    show();
    m_ui->findField->setFocus();
    m_ui->findField->lineEdit()->selectAll();
}

void SearchBar::toggle(Mode mode)
{
    // Only while typing in the bar: from the editor, the same keys are the
    // way back to it.
    if (m_mode == mode && isVisible() && isAncestorOf(QApplication::focusWidget())) {
        closeBar();
        return;
    }
    if (mode == Mode::Incremental)
        openIncremental();
    else
        openPower();
}

void SearchBar::closeBar()
{
    hide();
    clearHighlight();
    m_ui->selectionOnly->setChecked(false);
    setState(State::None);
    m_editor->setFocus();
}

void SearchBar::incrementalSearch()
{
    const sql::TextSearch s = search();
    if (s.pattern().isEmpty()) {
        m_editor->selectRange(m_start, m_start);
        clearHighlight();
        setState(State::None);
        return;
    }
    const QByteArray text = m_editor->utf8Text();
    auto match = s.findForward(text, m_start, false);
    const bool wrapped = !match;
    if (!match)
        match = s.findForward(text, 0, false);
    if (!match) {
        m_editor->selectRange(m_start, m_start);
        clearHighlight();
        setState(State::NotFound, tr("Not found"));
        return;
    }
    m_editor->selectRange(match->from, match->to);
    highlight(s);
    setState(State::Found, wrapped ? tr("Continued from the top") : QString());
}

bool SearchBar::findNext()
{
    return find(false, search());
}

bool SearchBar::findPrevious()
{
    return find(true, search());
}

bool SearchBar::find(bool backward, const sql::TextSearch &s)
{
    if (s.pattern().isEmpty()) {
        if (!isVisible())
            openIncremental();
        return false;
    }
    if (!s.isValid()) {
        show();
        setState(State::Invalid, tr("Invalid regular expression: %1").arg(s.errorString()));
        return false;
    }
    remember();

    const QByteArray text = m_editor->utf8Text();
    const auto [from, to] = range();
    const auto [selectionFrom, selectionTo] = m_editor->selectionRange();
    std::optional<sql::TextSearch::Match> match;
    bool wrapped = false;
    if (backward) {
        match = s.findBackward(text, selectionFrom, from, to);
        if (!match) {
            match = s.findBackward(text, to + 1, from, to);
            wrapped = match.has_value();
        }
    } else {
        // An empty selection may be an empty match, like ^, found last time.
        match = s.findForward(text, selectionTo, selectionFrom == selectionTo, from, to);
        if (!match) {
            match = s.findForward(text, from, false, from, to);
            wrapped = match.has_value();
        }
    }
    if (!match) {
        clearHighlight();
        setState(State::NotFound, tr("Not found"));
        return false;
    }
    m_editor->selectRange(match->from, match->to);
    m_start = match->from;
    highlight(s);
    if (!wrapped)
        setState(State::Found);
    else
        setState(State::Found,
                 backward ? tr("Continued from the bottom") : tr("Continued from the top"));
    return true;
}

bool SearchBar::findSelected(bool backward)
{
    auto [from, to] = m_editor->selectionRange();
    if (from == to) {
        const auto pos = static_cast<unsigned long>(m_editor->cursorPosition());
        from
            = qsizetype(m_editor->SendScintilla(QsciScintillaBase::SCI_WORDSTARTPOSITION, pos, 1L));
        to = qsizetype(m_editor->SendScintilla(QsciScintillaBase::SCI_WORDENDPOSITION, pos, 1L));
        if (from == to)
            return false;
        // Searching goes on from the word, not from inside it.
        m_editor->selectRange(from, to);
    }
    const QByteArray selected = m_editor->utf8Text().sliced(from, to - from);
    if (hasLineBreak(selected))
        return false;
    const QString text = QString::fromUtf8(selected);
    m_ui->findField->setEditText(text);
    return find(
        backward,
        sql::TextSearch(text, sql::TextSearch::Mode::PlainText, m_ui->matchCase->isChecked()));
}

void SearchBar::replace()
{
    const sql::TextSearch s = search();
    if (!s.isValid() || m_editor->isReadOnly()) {
        find(false, s); // Says what is wrong.
        return;
    }
    const QByteArray text = m_editor->utf8Text();
    const auto [from, to] = range();
    const auto [selectionFrom, selectionTo] = m_editor->selectionRange();
    const auto match = s.findForward(text, selectionFrom, false, from, to);
    // Only what is selected, and only if it is a match: else Replace finds
    // the next one first, to be seen before it is replaced.
    if (match && match->from == selectionFrom && match->to == selectionTo) {
        remember();
        const QByteArray with = s.replacement(*match, m_ui->replaceField->currentText(), newline());
        const auto scope
            = m_ui->selectionOnly->isChecked() ? m_editor->searchScope() : std::nullopt;
        m_editor->replaceRange(match->from, match->to, with);
        // Set again, as text replaced at its very start would fall outside.
        if (scope)
            m_editor->setSearchScope(
                std::pair(scope->first, scope->second + with.size() - (match->to - match->from)));
        m_editor->setCursorPosition(match->from + with.size());
    }
    find(false, s);
}

int SearchBar::replaceAll()
{
    const sql::TextSearch s = search();
    if (!s.isValid() || m_editor->isReadOnly()) {
        find(false, s);
        return 0;
    }
    remember();
    const auto [from, to] = range();
    const std::vector<sql::TextSearch::Match> matches = s.findAll(m_editor->utf8Text(), from, to);
    if (matches.empty()) {
        clearHighlight();
        setState(State::NotFound, tr("Not found"));
        return 0;
    }
    const QString with = m_ui->replaceField->currentText();
    const QString eol = newline();
    qsizetype growth = 0;
    m_editor->beginUndoAction();
    // From the end, so that the positions before stay where they were.
    for (auto it = matches.rbegin(); it != matches.rend(); ++it) {
        const QByteArray replacement = s.replacement(*it, with, eol);
        m_editor->replaceRange(it->from, it->to, replacement);
        growth += replacement.size() - (it->to - it->from);
    }
    m_editor->endUndoAction();
    if (m_ui->selectionOnly->isChecked() && m_editor->searchScope())
        m_editor->setSearchScope(std::pair(from, to + growth));
    highlight(s);
    const int count = int(matches.size());
    setState(State::Found, tr("%n replacement(s)", nullptr, count));
    return count;
}

int SearchBar::findAll()
{
    const sql::TextSearch s = search();
    if (!s.isValid()) {
        find(false, s);
        return 0;
    }
    remember();
    const auto [from, to] = range();
    const int count = int(s.findAll(m_editor->utf8Text(), from, to).size());
    highlight(s);
    if (count == 0)
        setState(State::NotFound, tr("Not found"));
    else
        setState(State::Found, tr("%n match(es)", nullptr, count));
    return count;
}

std::pair<qsizetype, qsizetype> SearchBar::range() const
{
    if (m_mode == Mode::Power && m_ui->selectionOnly->isChecked()) {
        if (const auto scope = m_editor->searchScope())
            return *scope;
    }
    return {0, m_editor->length()};
}

void SearchBar::setScopeFromSelection()
{
    const auto [from, to] = m_editor->selectionRange();
    if (from == to) {
        // Nothing selected, nothing to keep to.
        const QSignalBlocker blocker(m_ui->selectionOnly);
        m_ui->selectionOnly->setChecked(false);
        return;
    }
    m_editor->setSearchScope(std::pair(from, to));
    // The scope stays marked; searching starts at its top.
    m_editor->selectRange(from, from);
    if (m_highlighting)
        highlight(m_highlighted);
}

void SearchBar::highlight(const sql::TextSearch &s)
{
    // A bar that is not there could not take the highlight away again.
    if (!isVisible())
        return;
    m_highlighted = s;
    m_highlighting = true;
    const auto [from, to] = range();
    std::vector<std::pair<qsizetype, qsizetype>> ranges;
    for (const sql::TextSearch::Match &m : s.findAll(m_editor->utf8Text(), from, to))
        ranges.emplace_back(m.from, m.to);
    m_editor->setSearchMatches(ranges);
}

void SearchBar::clearHighlight()
{
    m_highlighting = false;
    m_highlightTimer.stop();
    m_editor->setSearchMatches({});
}

void SearchBar::setState(State state, const QString &message)
{
    m_state = state;
    m_ui->status->setText(message);
    // Only the base color: the rest still follows the application's palette.
    QPalette colors;
    if (state != State::None) {
        const QColor tint
            = state == State::Found ? QColor(0x3c, 0xb3, 0x71) : QColor(0xe0, 0x40, 0x40);
        colors.setColor(QPalette::Base, mix(palette().color(QPalette::Base), tint, 0.3));
    }
    m_ui->findField->setPalette(colors);
}

void SearchBar::remember()
{
    auto keep = [](QComboBox *combo, const QString &key) {
        const QString text = combo->currentText();
        if (text.isEmpty() || (combo->count() > 0 && combo->itemText(0) == text))
            return;
        QStringList items {text};
        for (int i = 0; i < combo->count() && items.size() < MaxHistory; ++i) {
            if (combo->itemText(i) != text)
                items << combo->itemText(i);
        }
        const int cursor = combo->lineEdit()->cursorPosition();
        const QSignalBlocker blocker(combo);
        combo->clear();
        combo->addItems(items);
        combo->setCurrentIndex(0);
        combo->lineEdit()->setCursorPosition(cursor);
        QSettings().setValue(key, items);
    };
    keep(m_ui->findField, PatternsKey);
    if (m_mode == Mode::Power)
        keep(m_ui->replaceField, ReplacementsKey);
}

QString SearchBar::newline() const
{
    switch (m_editor->eolMode()) {
    case QsciScintilla::EolWindows:
        return QStringLiteral("\r\n");
    case QsciScintilla::EolMac:
        return QStringLiteral("\r");
    case QsciScintilla::EolUnix:
        break;
    }
    return QStringLiteral("\n");
}

// The fields have their own Esc in the event filter; this is for the
// buttons and boxes, which leave it to their parent.
void SearchBar::keyPressEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_Escape && event->modifiers() == Qt::NoModifier) {
        closeBar();
        return;
    }
    QWidget::keyPressEvent(event);
}

bool SearchBar::eventFilter(QObject *watched, QEvent *event)
{
    if (event->type() != QEvent::KeyPress)
        return QWidget::eventFilter(watched, event);
    const auto *key = static_cast<QKeyEvent *>(event);
    if (watched == m_editor) {
        // Esc in the editor closes the bar too, unless it closes completion.
        if (key->key() == Qt::Key_Escape && key->modifiers() == Qt::NoModifier && isVisible()
            && !m_editor->completionPopup()->isVisible()) {
            closeBar();
            return true;
        }
        return false;
    }
    switch (key->key()) {
    case Qt::Key_Return:
    case Qt::Key_Enter:
        if (watched == m_ui->replaceField->lineEdit())
            replace();
        else if (key->modifiers() & Qt::ShiftModifier)
            findPrevious();
        else
            findNext();
        return true;
    case Qt::Key_Escape:
        closeBar();
        return true;
    default:
        return QWidget::eventFilter(watched, event);
    }
}

} // namespace sqliteman
