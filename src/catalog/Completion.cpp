// SPDX-FileCopyrightText: 2026 Petr Vanek
// SPDX-License-Identifier: GPL-3.0-or-later

#include "catalog/Completion.h"

#include "catalog/Pragmas.h"
#include "catalog/Snippets.h"
#include "sql/Keywords.h"
#include "sql/Lexer.h"

#include <QHash>
#include <QSet>

#include <algorithm>
#include <limits>

namespace sqliteman::catalog {

namespace {

using Context = Completion::Context;
using Kind = CompletionItem::Kind;
using sql::Token;
using sql::TokenKind;

// After _, space or . and at camelCase humps: learning_[p]ackage, Order[I]tems.
bool isWordStart(const QString &candidate, qsizetype i)
{
    if (i == 0)
        return true;
    const QChar before = candidate[i - 1];
    return before == QLatin1Char('_') || before == QLatin1Char(' ') || before == QLatin1Char('.')
        || (candidate[i].isUpper() && before.isLower());
}

struct Alignment
{
    int quality = 0; // 0 for no match at all, else 1 to 100.
    std::vector<qsizetype> positions;
};

// How well typed matches candidate, and where its letters landed. Matches
// are looked for in every position, not greedily, so a later run of letters
// can win over an early stray one: "leapa" takes "lea" and "pa" out of
// "learning_package" rather than stopping at the first "a".
Alignment align(const QString &typed, const QString &candidate)
{
    constexpr int Match = 16; // Any letter in the right order.
    constexpr int WordStart = 18; // A letter that starts a word.
    constexpr int Consecutive = 12; // Right after the previous match.
    constexpr int GapPenalty = 2; // Per letter skipped, up to the bonuses.
    constexpr int MaxPerChar = Match + WordStart + Consecutive;
    constexpr int NoMatch = std::numeric_limits<int>::min() / 2;

    const qsizetype n = typed.size(), m = candidate.size();
    if (n == 0 || n > m)
        return {};

    // score[i][j]: the best total for the first i + 1 typed letters, the
    // last of them matched at candidate[j]. parent[i][j] is where the letter
    // before it matched, for reading the positions back afterwards.
    std::vector<int> score(std::size_t(n * m), NoMatch);
    std::vector<qsizetype> parent(std::size_t(n * m), -1);
    for (qsizetype i = 0; i < n; ++i) {
        // The best the letter before this one reached left of j, kept as j
        // moves right and charged for every letter skipped on the way.
        int carried = i == 0 ? 0 : NoMatch;
        qsizetype carriedFrom = -1;
        for (qsizetype j = 0; j < m; ++j) {
            if (j > 0) {
                if (carried > NoMatch)
                    carried -= GapPenalty;
                const int before = i == 0 ? 0 : score[std::size_t((i - 1) * m + j - 1)];
                if (before > carried) {
                    carried = before;
                    carriedFrom = i == 0 ? -1 : j - 1;
                }
            }
            if (carried <= NoMatch || candidate[j].toLower() != typed[i].toLower())
                continue;
            int here = carried + Match;
            if (isWordStart(candidate, j))
                here += WordStart;
            if (i > 0 && carriedFrom == j - 1)
                here += Consecutive;
            score[std::size_t(i * m + j)] = here;
            parent[std::size_t(i * m + j)] = carriedFrom;
        }
    }

    const auto last = score.begin() + (n - 1) * m;
    const auto top = std::max_element(last, last + m);
    if (*top <= NoMatch)
        return {};
    Alignment out;
    out.quality = std::clamp(100 * *top / int(MaxPerChar * n), 1, 100);
    out.positions.resize(std::size_t(n));
    qsizetype at = top - last;
    for (qsizetype i = n - 1; i >= 0; --i) {
        out.positions[std::size_t(i)] = at;
        at = parent[std::size_t(i * m + at)];
    }
    return out;
}

// The score and, for highlighting, the letters of candidate it matched.
struct Match
{
    int score = 0;
    std::vector<qsizetype> positions;
};

Match match(const QString &typed, const QString &candidate)
{
    Match out;
    if (typed.isEmpty()) {
        out.score = 1;
        return out;
    }
    const QString t = typed.toLower();
    const QString c = candidate.toLower();
    auto run = [&](qsizetype from) {
        for (qsizetype i = 0; i < t.size(); ++i)
            out.positions.push_back(from + i);
    };
    if (c.startsWith(t)) {
        out.score = std::max(800, 1000 - int(c.size() - t.size()));
        run(0);
        return out;
    }

    const Alignment alignment = align(t, candidate);
    if (alignment.quality == 0)
        return out; // Not even scattered: no match at all.

    // An acronym, every letter starting a word: cnm for customer_name.
    std::vector<qsizetype> starts;
    for (qsizetype at = 0; const QChar ch : t) {
        qsizetype found = -1;
        for (qsizetype i = at; i < c.size() && found < 0; ++i)
            if (c[i] == ch && isWordStart(candidate, i))
                found = i;
        if (found < 0) {
            starts.clear();
            break;
        }
        starts.push_back(found);
        at = found + 1;
    }
    if (!starts.empty()) {
        out.score = 600 + alignment.quality;
        out.positions = std::move(starts);
        return out;
    }
    if (const qsizetype substring = c.indexOf(t); substring >= 0) {
        out.score = 500 - int(substring);
        run(substring);
        return out;
    }
    // Letters in order but scattered: how well they line up decides.
    out.score = 300 + alignment.quality;
    out.positions = std::move(alignment.positions);
    return out;
}

} // namespace

namespace {

class Tokens
{
public:
    explicit Tokens(const QByteArray &text) : m_text(text)
    {
        for (const Token &t : sql::tokenize(text)) {
            if (sql::isSignificant(t.kind))
                m_tokens.push_back(t);
        }
    }

    int size() const { return int(m_tokens.size()); }
    const Token &at(int i) const { return m_tokens[std::size_t(i)]; }
    QByteArrayView view(int i) const
    {
        return QByteArrayView(m_text).sliced(at(i).offset, at(i).length);
    }
    bool is(int i, const char *word) const
    {
        return i >= 0 && i < size()
            && (at(i).kind == TokenKind::Keyword || at(i).kind == TokenKind::Identifier)
            && view(i).compare(QByteArrayView(word), Qt::CaseInsensitive) == 0;
    }
    bool isPunct(int i, char c) const
    {
        return i >= 0 && i < size() && at(i).kind == TokenKind::Punctuation && view(i).front() == c;
    }
    bool isName(int i) const
    {
        return i >= 0 && i < size()
            && (at(i).kind == TokenKind::Identifier || at(i).kind == TokenKind::QuotedIdentifier);
    }
    QString name(int i) const { return QString::fromUtf8(sql::unquote(view(i))); }

private:
    const QByteArray &m_text;
    std::vector<Token> m_tokens;
};

struct TableRef
{
    QString schema;
    QString table;
};

// The tables the statement names after FROM, JOIN, UPDATE and INTO, by the
// alias they go by and by their own name.
QHash<QString, TableRef> tablesInScope(const Tokens &t)
{
    QHash<QString, TableRef> scope;
    for (int i = 0; i < t.size(); ++i) {
        if (!(t.is(i, "FROM") || t.is(i, "JOIN") || t.is(i, "UPDATE") || t.is(i, "INTO")
              || (t.isPunct(i, ',') && i > 0)))
            continue;
        int j = i + 1;
        if (!t.isName(j))
            continue;
        TableRef ref;
        ref.table = t.name(j);
        if (t.isPunct(j + 1, '.') && t.isName(j + 2)) {
            ref.schema = ref.table;
            ref.table = t.name(j + 2);
            j += 2;
        }
        ++j;
        if (t.is(j, "AS"))
            ++j;
        scope.insert(ref.table.toLower(), ref);
        if (t.isName(j))
            scope.insert(t.name(j).toLower(), ref);
    }
    return scope;
}

// Whether the comma at i separates tables of a FROM rather than columns.
bool inFromList(const Tokens &t, int i)
{
    int depth = 0;
    for (int j = i - 1; j >= 0; --j) {
        if (t.isPunct(j, ')'))
            ++depth;
        else if (t.isPunct(j, '('))
            --depth;
        if (depth != 0)
            continue;
        if (t.is(j, "FROM"))
            return true;
        if (t.is(j, "SELECT") || t.is(j, "WHERE") || t.is(j, "SET") || t.is(j, "ON")
            || t.is(j, "BY") || t.is(j, "VALUES"))
            return false;
    }
    return false;
}

Context contextBefore(const Tokens &t, int before)
{
    if (before < 0)
        return Context::Keyword;
    if (t.is(before, "FROM") || t.is(before, "JOIN") || t.is(before, "INTO")
        || t.is(before, "UPDATE") || t.is(before, "TABLE") || t.is(before, "REFERENCES")
        || (t.is(before, "ON") && t.is(0, "CREATE")))
        return Context::Relation;
    if (t.is(before, "PRAGMA"))
        return Context::Pragma;
    if (t.isPunct(before, ',') && inFromList(t, before))
        return Context::Relation;
    const TokenKind kind = t.at(before).kind;
    // After a complete name, number or parenthesis: what may follow is a keyword.
    if (kind == TokenKind::Identifier || kind == TokenKind::QuotedIdentifier
        || kind == TokenKind::Number || kind == TokenKind::String || kind == TokenKind::Blob
        || t.isPunct(before, ')'))
        return Context::Keyword;
    return Context::Column;
}

// The keywords most statements are made of, ahead of the rare ones that
// match as well: WHERE before WHEN.
bool isCommonKeyword(const QString &word)
{
    static const QSet<QString> common {
        QStringLiteral("SELECT"), QStringLiteral("FROM"),     QStringLiteral("WHERE"),
        QStringLiteral("JOIN"),   QStringLiteral("LEFT"),     QStringLiteral("INNER"),
        QStringLiteral("ON"),     QStringLiteral("GROUP"),    QStringLiteral("ORDER"),
        QStringLiteral("BY"),     QStringLiteral("HAVING"),   QStringLiteral("LIMIT"),
        QStringLiteral("INSERT"), QStringLiteral("INTO"),     QStringLiteral("VALUES"),
        QStringLiteral("UPDATE"), QStringLiteral("SET"),      QStringLiteral("DELETE"),
        QStringLiteral("CREATE"), QStringLiteral("TABLE"),    QStringLiteral("AND"),
        QStringLiteral("OR"),     QStringLiteral("NOT"),      QStringLiteral("NULL"),
        QStringLiteral("AS"),     QStringLiteral("DISTINCT"), QStringLiteral("UNION")};
    return common.contains(word);
}

void add(std::vector<CompletionItem> &items, const QString &prefix, CompletionItem item)
{
    item.score = fuzzyScore(prefix, item.label);
    if (item.score <= 0)
        return;
    if (item.kind == Kind::Keyword && isCommonKeyword(item.label.toUpper()))
        item.score += 5;
    items.push_back(std::move(item));
}

int kindRank(Kind kind)
{
    switch (kind) {
    case Kind::Column:
        return 0;
    case Kind::Alias:
        return 1;
    case Kind::Table:
    case Kind::View:
        return 2;
    case Kind::Schema:
        return 3;
    case Kind::Pragma:
    case Kind::Function:
        return 4;
    case Kind::Snippet:
        return 5;
    case Kind::Keyword:
        return 6;
    }
    return 7;
}

void addObjects(std::vector<CompletionItem> &items, const QString &prefix,
                const DatabaseInfo &database, bool qualify)
{
    for (const SchemaObject &o : database.objects) {
        if (o.kind != ObjectKind::Table && o.kind != ObjectKind::VirtualTable
            && o.kind != ObjectKind::View)
            continue;
        if (o.system && !prefix.startsWith(QLatin1String("sqlite"), Qt::CaseInsensitive))
            continue;
        CompletionItem item;
        item.kind = o.kind == ObjectKind::View ? Kind::View : Kind::Table;
        item.label = o.name;
        item.insertText = qualify
            ? sql::identifier(database.name) + QLatin1Char('.') + sql::identifier(o.name)
            : sql::identifier(o.name);
        item.detail = database.name;
        add(items, prefix, std::move(item));
    }
}

void addColumns(std::vector<CompletionItem> &items, const QString &prefix,
                const SchemaObject &table)
{
    for (const ColumnInfo &c : table.columns) {
        CompletionItem item;
        item.kind = Kind::Column;
        item.label = c.name;
        item.insertText = sql::identifier(c.name);
        item.detail = c.type.isEmpty() ? table.name : table.name + QLatin1String(" · ") + c.type;
        add(items, prefix, std::move(item));
    }
}

} // namespace

QHash<QString, QString> tableAliases(const QByteArray &statement)
{
    QHash<QString, QString> aliases;
    const QHash<QString, TableRef> scope = tablesInScope(Tokens(statement));
    for (auto it = scope.cbegin(); it != scope.cend(); ++it)
        aliases.insert(it.key(), it.value().table);
    return aliases;
}

Completion complete(const QByteArray &statement, qsizetype cursor, const Snapshot &snapshot)
{
    Completion out;
    cursor = std::clamp<qsizetype>(cursor, 0, statement.size());
    out.replaceFrom = out.replaceTo = cursor;

    // Inside a comment or a string there is nothing to complete.
    for (const Token &t : sql::tokenize(statement)) {
        if (t.offset < cursor && cursor < t.end()
            && (t.kind == TokenKind::Comment || t.kind == TokenKind::String
                || t.kind == TokenKind::Blob))
            return out;
        if (t.kind == TokenKind::Comment && t.offset < cursor && cursor == t.end()
            && statement.mid(t.offset, 2) == "--")
            return out;
    }

    const Tokens t(statement);
    int word = -1; // The token the cursor is in or right after.
    for (int i = 0; i < t.size(); ++i) {
        const Token &tok = t.at(i);
        if (tok.offset < cursor && cursor <= tok.end()
            && (tok.kind == TokenKind::Identifier || tok.kind == TokenKind::Keyword
                || tok.kind == TokenKind::QuotedIdentifier)) {
            word = i;
            break;
        }
    }
    int before = -1; // The last token before the word.
    for (int i = 0; i < t.size(); ++i) {
        if (word >= 0 ? i < word : t.at(i).end() <= cursor)
            before = i;
    }
    if (word >= 0) {
        out.replaceFrom = t.at(word).offset;
        out.replaceTo = t.at(word).end();
        QByteArray typed = statement.mid(out.replaceFrom, cursor - out.replaceFrom);
        if (!typed.isEmpty()
            && (typed.front() == '"' || typed.front() == '[' || typed.front() == '`'))
            typed.remove(0, 1);
        out.prefix = QString::fromUtf8(typed);
    }

    std::vector<CompletionItem> &items = out.items;
    const QHash<QString, TableRef> scope = tablesInScope(t);

    if (t.isPunct(before, '.') && t.isName(before - 1)) {
        out.context = Context::Qualified;
        const QString qualifier = t.name(before - 1);
        if (const DatabaseInfo *database = snapshot.database(qualifier)) {
            addObjects(items, out.prefix, *database, false);
        } else {
            const TableRef ref = scope.value(qualifier.toLower(), TableRef {QString(), qualifier});
            if (const SchemaObject *table = snapshot.find(ref.schema, ref.table))
                addColumns(items, out.prefix, *table);
        }
    } else {
        out.context = contextBefore(t, before);
        switch (out.context) {
        case Context::Relation:
            for (const DatabaseInfo &d : snapshot.databases) {
                // main and temp need no qualifying; an attached one does.
                const bool attached
                    = d.name != QLatin1String("main") && d.name != QLatin1String("temp");
                addObjects(items, out.prefix, d, attached);
                if (attached)
                    add(items, out.prefix,
                        {Kind::Schema, d.name, sql::identifier(d.name), d.file, -1, 0});
            }
            break;
        case Context::Pragma:
            for (const PragmaInfo &p : pragmas())
                add(items, out.prefix, {Kind::Pragma, p.name, p.name, p.description, -1, 0});
            break;
        case Context::Column: {
            QSet<QString> seen;
            for (auto it = scope.cbegin(); it != scope.cend(); ++it) {
                const SchemaObject *table = snapshot.find(it->schema, it->table);
                if (!table)
                    continue;
                if (it.key() != table->name.toLower())
                    add(items, out.prefix, {Kind::Alias, it.key(), it.key(), table->name, -1, 0});
                if (seen.contains(table->schema + QLatin1Char('.') + table->name))
                    continue;
                seen.insert(table->schema + QLatin1Char('.') + table->name);
                addColumns(items, out.prefix, *table);
            }
            for (const QString &f : snapshot.functions)
                add(items, out.prefix,
                    {Kind::Function, f, f + QLatin1String("()"), QString(), int(f.size()) + 1, 0});
            [[fallthrough]];
        }
        case Context::Keyword: {
            // Written as it is being typed: "sel" goes on as "select".
            const bool lowercase = !out.prefix.isEmpty() && out.prefix == out.prefix.toLower();
            for (const QByteArray &k : sql::keywords()) {
                const QString keyword
                    = lowercase ? QString::fromLatin1(k).toLower() : QString::fromLatin1(k);
                add(items, out.prefix, {Kind::Keyword, keyword, keyword, QString(), -1, 0});
            }
            // Short words standing for whole statements. Worth offering first
            // when their abbreviation was typed, and not at all when nothing
            // was: a bare Ctrl+Space is a question about this statement, not
            // a menu.
            if (out.prefix.isEmpty())
                break;
            for (const Snippet &snippet : snippets()) {
                QString text = lowercase ? snippet.text.toLower() : snippet.text;
                const int caret = int(text.indexOf(Snippet::CaretMark));
                if (caret >= 0)
                    text.remove(caret, int(Snippet::CaretMark.size()));
                const std::size_t offered = items.size();
                add(items, out.prefix,
                    {Kind::Snippet, snippet.abbreviation, text, snippet.title, caret, 0});
                if (items.size() > offered)
                    items.back().score += 40;
            }
            break;
        }
        case Context::None:
        case Context::Qualified:
            break;
        }
    }

    std::stable_sort(items.begin(), items.end(),
                     [](const CompletionItem &a, const CompletionItem &b) {
                         if (a.score != b.score)
                             return a.score > b.score;
                         if (kindRank(a.kind) != kindRank(b.kind))
                             return kindRank(a.kind) < kindRank(b.kind);
                         return a.label.compare(b.label, Qt::CaseInsensitive) < 0;
                     });
    // The same name from two tables once, with the first table's detail.
    QSet<std::pair<int, QString>> unique;
    std::erase_if(items, [&](const CompletionItem &i) {
        const std::pair<int, QString> key {int(i.kind), i.insertText.toLower()};
        if (unique.contains(key))
            return true;
        unique.insert(key);
        return false;
    });
    return out;
}

int fuzzyScore(const QString &typed, const QString &candidate)
{
    return match(typed, candidate).score;
}

std::vector<qsizetype> fuzzyMatchPositions(const QString &typed, const QString &candidate)
{
    return match(typed, candidate).positions;
}

} // namespace sqliteman::catalog
