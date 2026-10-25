// SPDX-FileCopyrightText: 2026 Petr Vanek
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QWidget>

class QComboBox;
class QFileSystemModel;
class QLineEdit;
class QSettings;
class QTreeView;

namespace sqliteman {

// Files on disk, from the folder in the path field down, showing those that
// match the filter (scripts and databases by default). Opening one hands it
// to the main window, which opens a database or puts a script in an editor.
class FileBrowser : public QWidget
{
    Q_OBJECT

public:
    explicit FileBrowser(QSettings &settings, QWidget *parent = nullptr);

    QString path() const;
    // Shows the tree from a folder; false, and nothing changes, if it is not one.
    bool setPath(const QString &path);
    QString filter() const;
    // Space or semicolon separated patterns, like "*.sql *.db". "*.*"
    // means all files, with or without a dot in the name.
    void setFilter(const QString &filter);
    QComboBox *filterBox() const { return m_filter; }

    QFileSystemModel *model() const { return m_model; }
    QTreeView *view() const { return m_view; }
    QLineEdit *pathEdit() const { return m_path; }

Q_SIGNALS:
    void fileActivated(const QString &path);

private:
    void applyPath();
    void showContextMenu(const QPoint &pos);

    QSettings &m_settings;
    QLineEdit *m_path = nullptr;
    QComboBox *m_filter = nullptr;
    QFileSystemModel *m_model = nullptr;
    QTreeView *m_view = nullptr;
};

} // namespace sqliteman
