// SPDX-FileCopyrightText: 2007-2026 Petr Vanek
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "config/Preferences.h"

#include <QDialog>

#include <memory>

namespace Ui {
class PreferencesDialog;
}

class QPushButton;

namespace sqliteman {

// What the program goes by, in one place: OK saves it and every editor and
// grid follows at once.
class PreferencesDialog : public QDialog
{
    Q_OBJECT

public:
    explicit PreferencesDialog(const config::Preferences &preferences, QWidget *parent = nullptr);
    ~PreferencesDialog() override;

    void setPreferences(const config::Preferences &preferences);
    config::Preferences preferences() const;

private:
    void showColor(QPushButton *button, const QColor &color);
    void showFont();
    void addSnippet(const QString &key, const QString &text);

    std::unique_ptr<Ui::PreferencesDialog> m_ui; // Layout in PreferencesDialog.ui.
    QColor m_nullColor;
    QColor m_blobColor;
    QString m_font;
};

} // namespace sqliteman
