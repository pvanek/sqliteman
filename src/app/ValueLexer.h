// SPDX-FileCopyrightText: 2026 Petr Vanek
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "catalog/ValueFormat.h"

#include "Qsci.h"

#include <vector>

namespace sqliteman {

// Highlights a single value by the tokens catalog::tokenize() found in it.
// The value does not change while it is shown, so it is tokenized once,
// whole, and each range Scintilla asks for is styled from that.
class ValueLexer : public QsciLexerCustom
{
    Q_OBJECT

public:
    enum Style {
        Default, // Between tokens, and all of a plain text.
        // Then one per catalog::ValueToken::Kind, in its order.
        StyleCount = Default + 1 + catalog::ValueToken::Error + 1,
    };
    static int styleFor(catalog::ValueToken::Kind kind) { return Default + 1 + int(kind); }

    explicit ValueLexer(QObject *parent = nullptr);

    const char *language() const override { return "Value"; }
    QString description(int style) const override;
    void styleText(int start, int end) override;

    // The tokens of the text about to be shown, in order.
    void setTokens(std::vector<catalog::ValueToken> tokens);
    // Colours from the application palette, light or dark; again after it
    // changes.
    void applyPalette();

private:
    std::vector<catalog::ValueToken> m_tokens;
};

} // namespace sqliteman
