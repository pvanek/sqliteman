// SPDX-FileCopyrightText: 2007-2026 Petr Vanek
// SPDX-License-Identifier: GPL-3.0-or-later

#include "catalog/TableDefinition.h"

#include "sql/Keywords.h"
#include "sql/Lexer.h"

namespace sqliteman::catalog {

namespace {

using sql::Token;
using sql::TokenKind;

// The significant tokens of a statement, and the text they point into.
class Tokens
{
public:
    explicit Tokens(const QString &text) : m_utf8(text.toUtf8())
    {
        for (const Token &t : sql::tokenize(m_utf8)) {
            if (sql::isSignificant(t.kind))
                m_tokens.push_back(t);
        }
    }

    int size() const { return int(m_tokens.size()); }
    const Token &at(int i) const { return m_tokens[std::size_t(i)]; }
    QByteArrayView view(int i) const
    {
        return QByteArrayView(m_utf8).sliced(at(i).offset, at(i).length);
    }
    bool is(int i, const char *word) const
    {
        return i < size()
            && (at(i).kind == TokenKind::Keyword || at(i).kind == TokenKind::Identifier)
            && view(i).compare(QByteArrayView(word), Qt::CaseInsensitive) == 0;
    }
    bool isPunct(int i, char c) const
    {
        return i < size() && at(i).kind == TokenKind::Punctuation && view(i).front() == c;
    }
    // The source text from token from up to and including token to - 1, as written.
    QString text(int from, int to) const
    {
        if (from >= to)
            return {};
        const qsizetype start = at(from).offset;
        return QString::fromUtf8(m_utf8.mid(start, at(to - 1).end() - start));
    }
    QString name(int i) const { return QString::fromUtf8(sql::unquote(view(i))); }
    // Past the parenthesis matching the one at i.
    int skipParens(int i) const
    {
        int depth = 0;
        for (; i < size(); ++i) {
            if (isPunct(i, '('))
                ++depth;
            else if (isPunct(i, ')') && --depth == 0)
                return i + 1;
        }
        return size();
    }

private:
    QByteArray m_utf8;
    std::vector<Token> m_tokens;
};

bool startsConstraint(const Tokens &t, int i)
{
    for (const char *word : {"CONSTRAINT", "PRIMARY", "NOT", "NULL", "UNIQUE", "CHECK", "DEFAULT",
                             "COLLATE", "REFERENCES", "GENERATED", "AS"}) {
        if (t.is(i, word))
            return true;
    }
    return false;
}

// Where the column constraint starting at i ends.
int constraintEnd(const Tokens &t, int i, int end)
{
    if (t.is(i, "CONSTRAINT")) {
        // Its name, then the constraint it names.
        return i + 2 < end ? constraintEnd(t, i + 2, end) : end;
    }
    if (t.is(i, "DEFAULT")) {
        int j = i + 1;
        if (t.isPunct(j, '('))
            return std::min(t.skipParens(j), end);
        if (j < end && t.at(j).kind == TokenKind::Operator) // A signed number.
            ++j;
        return std::min(j + 1, end);
    }
    if (t.is(i, "CHECK") || (t.is(i, "AS") && t.isPunct(i + 1, '('))) {
        int j = std::min(t.skipParens(i + 1), end);
        if (t.is(i, "AS") && (t.is(j, "STORED") || t.is(j, "VIRTUAL")))
            ++j;
        return j;
    }
    if (t.is(i, "COLLATE"))
        return std::min(i + 2, end);
    if (t.is(i, "GENERATED")) {
        // GENERATED ALWAYS AS ( ... ) [STORED | VIRTUAL]
        int j = i + 1;
        while (j < end && !t.isPunct(j, '('))
            ++j;
        j = std::min(t.skipParens(j), end);
        if (t.is(j, "STORED") || t.is(j, "VIRTUAL"))
            ++j;
        return j;
    }
    const bool references = t.is(i, "REFERENCES");
    int depth = 0;
    int j = i + 1;
    if (t.is(i, "NOT") || t.is(i, "PRIMARY"))
        ++j; // NOT NULL, PRIMARY KEY: the second word is part of it.
    for (; j < end; ++j) {
        if (t.isPunct(j, '('))
            ++depth;
        else if (t.isPunct(j, ')'))
            --depth;
        if (depth > 0 || !startsConstraint(t, j))
            continue;
        // ON DELETE SET NULL, SET DEFAULT and NOT DEFERRABLE belong to a
        // foreign key.
        if (references && (t.is(j - 1, "SET") || (t.is(j, "NOT") && t.is(j + 1, "DEFERRABLE"))))
            continue;
        break;
    }
    return j;
}

void parseColumnConstraints(const Tokens &t, int i, int end, ColumnDef &c)
{
    QStringList extra;
    while (i < end) {
        const int stop = constraintEnd(t, i, end);
        const int n = stop - i;
        if (t.is(i, "PRIMARY") && (n == 2 || (n == 3 && t.is(i + 2, "AUTOINCREMENT")))) {
            c.primaryKey = true;
            c.autoincrement = n == 3;
        } else if (t.is(i, "NOT") && n == 2 && t.is(i + 1, "NULL")) {
            c.notNull = true;
        } else if (t.is(i, "NULL") && n == 1) {
            // Allowed, and the default anyway.
        } else if (t.is(i, "UNIQUE") && n == 1) {
            c.unique = true;
        } else if (t.is(i, "DEFAULT") && n > 1) {
            c.defaultValue = t.text(i + 1, stop);
        } else if (t.is(i, "CHECK") && n >= 3 && t.isPunct(stop - 1, ')')) {
            c.check = t.text(i + 2, stop - 1);
        } else if (t.is(i, "COLLATE") && n == 2) {
            c.collate = t.text(i + 1, stop);
        } else {
            extra << t.text(i, stop);
        }
        i = std::max(stop, i + 1);
    }
    c.extra = extra.join(QLatin1Char(' '));
}

ColumnDef parseColumn(const Tokens &t, int from, int to)
{
    ColumnDef c;
    c.name = t.name(from);
    int i = from + 1;
    const int typeStart = i;
    while (i < to && !startsConstraint(t, i)) {
        if (t.isPunct(i, '('))
            i = std::min(t.skipParens(i), to);
        else
            ++i;
    }
    c.type = t.text(typeStart, i);
    parseColumnConstraints(t, i, to, c);
    return c;
}

bool isNumber(const QString &text)
{
    const QByteArray utf8 = text.toUtf8();
    const auto tokens = sql::tokenize(utf8);
    int significant = 0;
    bool number = false;
    for (const Token &tok : tokens) {
        if (!sql::isSignificant(tok.kind))
            continue;
        ++significant;
        if (tok.kind == TokenKind::Number)
            number = true;
        else if (!(significant == 1 && tok.kind == TokenKind::Operator
                   && (utf8[tok.offset] == '-' || utf8[tok.offset] == '+')))
            return false;
    }
    return number && significant <= 2;
}

} // namespace

bool ColumnDef::sameDefinition(const ColumnDef &other) const
{
    return type == other.type && primaryKey == other.primaryKey
        && autoincrement == other.autoincrement && notNull == other.notNull
        && unique == other.unique && defaultValue == other.defaultValue && check == other.check
        && collate == other.collate && extra == other.extra;
}

QString columnDefinition(const ColumnDef &c)
{
    QString out = sql::identifier(c.name);
    if (!c.type.trimmed().isEmpty())
        out += QLatin1Char(' ') + c.type.trimmed();
    if (c.primaryKey) {
        out += QLatin1String(" PRIMARY KEY");
        if (c.autoincrement)
            out += QLatin1String(" AUTOINCREMENT");
    }
    if (c.notNull)
        out += QLatin1String(" NOT NULL");
    if (c.unique)
        out += QLatin1String(" UNIQUE");
    if (!c.defaultValue.trimmed().isEmpty())
        out += QLatin1String(" DEFAULT ") + c.defaultValue.trimmed();
    if (!c.check.trimmed().isEmpty())
        out += QLatin1String(" CHECK (") + c.check.trimmed() + QLatin1Char(')');
    if (!c.collate.trimmed().isEmpty())
        out += QLatin1String(" COLLATE ") + c.collate.trimmed();
    if (!c.extra.trimmed().isEmpty())
        out += QLatin1Char(' ') + c.extra.trimmed();
    return out;
}

QString createTableStatement(const TableDef &table)
{
    QString out = QStringLiteral("CREATE TABLE ");
    if (table.ifNotExists)
        out += QLatin1String("IF NOT EXISTS ");
    if (!table.schema.isEmpty())
        out += sql::identifier(table.schema) + QLatin1Char('.');
    out += sql::identifier(table.name) + QLatin1String(" (\n");
    QStringList lines;
    for (const ColumnDef &c : table.columns)
        lines << QLatin1String("    ") + columnDefinition(c);
    for (const QString &constraint : table.constraints) {
        if (!constraint.trimmed().isEmpty())
            lines << QLatin1String("    ") + constraint.trimmed();
    }
    out += lines.join(QLatin1String(",\n")) + QLatin1String("\n)");
    QStringList options;
    if (table.withoutRowid)
        options << QStringLiteral("WITHOUT ROWID");
    if (table.strict)
        options << QStringLiteral("STRICT");
    if (!options.isEmpty())
        out += QLatin1Char(' ') + options.join(QLatin1String(", "));
    return out;
}

std::optional<TableDef> parseCreateTable(const QString &sql)
{
    const Tokens t(sql);
    int i = 0;
    if (!t.is(i++, "CREATE"))
        return std::nullopt;
    if (t.is(i, "TEMP") || t.is(i, "TEMPORARY"))
        ++i;
    if (!t.is(i++, "TABLE"))
        return std::nullopt;
    TableDef table;
    if (t.is(i, "IF") && t.is(i + 1, "NOT") && t.is(i + 2, "EXISTS")) {
        table.ifNotExists = true;
        i += 3;
    }
    if (i >= t.size())
        return std::nullopt;
    table.name = t.name(i++);
    if (t.isPunct(i, '.') && i + 1 < t.size()) {
        table.schema = table.name;
        table.name = t.name(i + 1);
        i += 2;
    }
    if (!t.isPunct(i, '('))
        return std::nullopt; // CREATE TABLE ... AS SELECT
    const int close = t.skipParens(i) - 1;
    if (close >= t.size() || !t.isPunct(close, ')'))
        return std::nullopt;

    int from = i + 1;
    int depth = 0;
    for (int j = i + 1; j <= close; ++j) {
        if (t.isPunct(j, '('))
            ++depth;
        else if (t.isPunct(j, ')') && j < close)
            --depth;
        if ((depth == 0 && t.isPunct(j, ',')) || j == close) {
            if (j > from) {
                if (t.is(from, "CONSTRAINT") || t.is(from, "PRIMARY") || t.is(from, "UNIQUE")
                    || t.is(from, "CHECK") || t.is(from, "FOREIGN"))
                    table.constraints << t.text(from, j);
                else
                    table.columns.push_back(parseColumn(t, from, j));
            }
            from = j + 1;
        }
    }
    for (int j = close + 1; j < t.size(); ++j) {
        if (t.is(j, "WITHOUT") && t.is(j + 1, "ROWID"))
            table.withoutRowid = true;
        else if (t.is(j, "STRICT"))
            table.strict = true;
    }
    if (table.columns.empty())
        return std::nullopt;
    return table;
}

QString defaultExpression(const QString &userText)
{
    const QString text = userText.trimmed();
    if (text.isEmpty())
        return {};
    if (isNumber(text) || text.startsWith(QLatin1Char('\'')) || text.startsWith(QLatin1Char('('))
        || text.startsWith(QLatin1String("x'"), Qt::CaseInsensitive))
        return text;
    for (const char *keyword :
         {"NULL", "TRUE", "FALSE", "CURRENT_TIME", "CURRENT_DATE", "CURRENT_TIMESTAMP"}) {
        if (text.compare(QLatin1String(keyword), Qt::CaseInsensitive) == 0)
            return text;
    }
    QString quoted = text;
    quoted.replace(QLatin1Char('\''), QLatin1String("''"));
    return QLatin1Char('\'') + quoted + QLatin1Char('\'');
}

QString columnsConstraint(const QString &keyword, const QStringList &columns)
{
    QStringList names;
    for (const QString &c : columns)
        names << sql::identifier(c);
    return keyword + QLatin1String(" (") + names.join(QLatin1String(", ")) + QLatin1Char(')');
}

} // namespace sqliteman::catalog
