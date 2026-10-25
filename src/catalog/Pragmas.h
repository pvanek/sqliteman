// SPDX-FileCopyrightText: 2007-2026 Petr Vanek
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QByteArray>
#include <QString>
#include <QStringList>

#include <vector>

namespace sqliteman::catalog {

// The PRAGMAs worth looking at and setting from the pragma editor.
struct PragmaInfo
{
    enum class Kind {
        Boolean,
        Integer,
        Choice, // One of choices, set by its name; read back as a number or a name.
        Text,
        ReadOnly,
    };

    QString name;
    Kind kind = Kind::Integer;
    bool perDatabase = true; // Takes main., temp. or an attached name; else the connection's.
    QStringList choices; // For Choice: the names, in the order of their numbers.
    QString description;
};

const std::vector<PragmaInfo> &pragmas();
const PragmaInfo *pragma(const QString &name);

// One statement per pragma, in the order of pragmas(), each giving one row
// with the pragma's value; the results come back in that order.
QByteArray readPragmasQuery(const QString &schema);
// PRAGMA schema.name = value, with value as the user gave it: a choice by
// its name, a boolean as ON or OFF, a number as it is.
QByteArray setPragmaStatement(const QString &schema, const PragmaInfo &pragma,
                              const QString &value);
// The value as SQLite reports it, with a choice's name next to its number.
QString displayValue(const PragmaInfo &pragma, const QString &raw);

} // namespace sqliteman::catalog
