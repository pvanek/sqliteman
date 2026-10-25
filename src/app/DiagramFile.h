// SPDX-FileCopyrightText: 2026 Petr Vanek
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QByteArray>
#include <QPalette>
#include <QSizeF>
#include <QString>

#include <functional>

class QPainter;
class QWidget;

namespace sqliteman {

// Saving what a view draws, the ER diagram's and the plan's: as a picture,
// or as text another tool draws.

// Paints the picture, as big as it is in the view, into target.
using PictureRenderer = std::function<void(QPainter *painter, const QRectF &target)>;

// Whether the file name's suffix is .svg, .png or .pdf.
bool isPictureFile(const QString &path);
// Saves the picture as the suffix says. dpi is the one the view was laid
// out at, which fonts set in points are scaled by. A PNG is drawn at twice
// the size, for screens that need it, unless that would be too big.
bool savePicture(const QString &path, const QSizeF &size, int dpi, const PictureRenderer &render,
                 QString *error);
bool saveText(const QString &path, const QByteArray &content, QString *error);

// Black on white, whatever the screen shows: a file is read away from the
// program's colours.
QPalette exportPalette();

// Asks where to save, offering name with the suffix of the filter chosen,
// in the directory last saved to, then saves with save() and says if that
// failed.
void exportToFile(QWidget *parent, const QString &title, const QString &name, const QString &filter,
                  const std::function<bool(const QString &path, QString *error)> &save);

} // namespace sqliteman
