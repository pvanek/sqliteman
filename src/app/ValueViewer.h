// SPDX-FileCopyrightText: 2026 Petr Vanek
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "catalog/ValueFormat.h"
#include "sqlite/Value.h"

#include <QDialog>

#include <memory>

namespace Ui {
class ValueViewer;
}

namespace sqliteman {

class ResultTextView;
class ValueLexer;

// One value of a result in a window of its own: a long text or a JSON
// document formatted and highlighted, a blob as an image when it is one and
// in hex. Editable when its cell is: the text, NULL, or a file's content as
// a blob, which also saves back to a file. The value is a copy, so the
// window stays as it was when the query runs again.
class ValueViewer : public QDialog
{
    Q_OBJECT

public:
    // title says where the value is from, such as its column and row.
    ValueViewer(const QString &title, const sqlite::Value &value, bool editable,
                QWidget *parent = nullptr);
    ~ValueViewer() override;

    bool isEditable() const { return m_editable; }
    // The value as it stands now: NULL, a blob, or the text as typed.
    sqlite::Value value() const;
    // Whether value() is text the user typed, for a column's affinity to
    // make a number of, rather than a value to store as it is.
    bool isTypedText() const;
    bool isModified() const;
    // Takes a file's content as the value; false if it cannot be read.
    bool loadFile(const QString &path);
    bool saveFile(const QString &path) const;

    catalog::ValueSyntax syntax() const;
    void setSyntax(catalog::ValueSyntax syntax);
    bool isFormatted() const;
    void setFormatted(bool formatted);
    bool wrapsLines() const;
    void setWrapLines(bool wrap);
    // What is on show, formatted or not.
    QString text() const;
    // The size of the value, or why it could not be formatted.
    QString status() const;
    ResultTextView *editor() const;
    // Whether the value is shown as an image.
    bool showsImage() const;

Q_SIGNALS:
    // OK was pressed on an editable value.
    void valueAccepted(const sqliteman::sqlite::Value &value, bool typedText);

protected:
    void changeEvent(QEvent *event) override;

private:
    void showValue();
    void updateNull();

    std::unique_ptr<Ui::ValueViewer> m_ui; // Layout in ValueViewer.ui.
    ValueLexer *m_lexer = nullptr;
    sqlite::Value m_original;
    sqlite::Value m_value; // What is shown; a loaded file replaces it.
    bool m_editable = false;
    bool m_textEdited = false;
};

} // namespace sqliteman
