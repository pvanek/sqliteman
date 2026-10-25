// SPDX-FileCopyrightText: 2026 Petr Vanek
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "Qsci.h"

namespace sqliteman {

// The result as an ASCII table, read-only. Scintilla rather than a plain
// text edit for one reason: Alt+drag selects a block, so single columns of
// the table can be copied out.
class ResultTextView : public QsciScintilla
{
    Q_OBJECT

public:
    explicit ResultTextView(QWidget *parent = nullptr);

    void setText(const QString &text) override;

    // Selects the rectangle between two positions, as Alt+drag does.
    void selectBlock(qsizetype anchor, qsizetype caret);
    // What Copy would put on the clipboard: for a block, its lines joined.
    // QsciScintilla::selectedText() covers only a plain selection.
    QString selection() const;

protected:
    void changeEvent(QEvent *event) override;

private:
    void applyTheme();
    void updateScrollWidth();

    qsizetype m_longestLine = 0; // In characters.
};

} // namespace sqliteman
