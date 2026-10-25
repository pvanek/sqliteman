// SPDX-FileCopyrightText: 2026 Petr Vanek
// SPDX-License-Identifier: GPL-3.0-or-later

#include "Icons.h"

#include <QApplication>
#include <QPainter>
#include <QPainterPath>
#include <QPixmap>
#include <QStyle>

#include <algorithm>

namespace sqliteman::Icons {

namespace {

QIcon themed(const QString &name, QStyle::StandardPixmap fallback)
{
    return QIcon::fromTheme(name, QApplication::style()->standardIcon(fallback));
}

// A cylinder, the usual picture of a database, in a color.
QIcon cylinder(const QColor &base)
{
    QIcon icon;
    for (const int size : {16, 22, 32, 48}) {
        QPixmap pixmap(size, size);
        pixmap.fill(Qt::transparent);
        QPainter painter(&pixmap);
        painter.setRenderHint(QPainter::Antialiasing);
        const qreal pen = std::max(1.0, size / 16.0);
        painter.setPen(QPen(base.darker(150), pen));

        // The body, then its top, and a band halfway down.
        const qreal left = size * 0.18, right = size * 0.82, top = size * 0.12,
                    bottom = size * 0.88;
        const qreal lid = size * 0.22; // Height of the elliptic ends.
        QPainterPath body;
        body.moveTo(left, top + lid / 2);
        body.lineTo(left, bottom - lid / 2);
        body.arcTo(QRectF(left, bottom - lid, right - left, lid), 180, 180);
        body.lineTo(right, top + lid / 2);
        body.arcTo(QRectF(left, top, right - left, lid), 0, -180);
        painter.setBrush(base);
        painter.drawPath(body);
        painter.setBrush(base.lighter(140));
        painter.drawEllipse(QRectF(left, top, right - left, lid));
        painter.setBrush(Qt::NoBrush);
        const qreal middle = (top + bottom) / 2 - lid / 2;
        painter.drawArc(QRectF(left, middle, right - left, lid), 180 * 16, 180 * 16);
        icon.addPixmap(pixmap);
    }
    return icon;
}

} // namespace

QIcon database(bool open)
{
    // SQLite's own blue, greyed out while the file is closed.
    static const QIcon opened = cylinder(QColor(0x0f, 0x80, 0xcc));
    static const QIcon closed = cylinder(QColor(0x9e, 0x9e, 0x9e));
    return open ? opened : closed;
}

QIcon object(catalog::ObjectKind kind)
{
    using K = catalog::ObjectKind;
    switch (kind) {
    case K::Database:
        return themed(QStringLiteral("server-database"), QStyle::SP_DriveHDIcon);
    case K::Table:
        return themed(QStringLiteral("table"), QStyle::SP_FileDialogDetailedView);
    case K::VirtualTable:
        return themed(QStringLiteral("view-form-table"), QStyle::SP_FileDialogDetailedView);
    case K::View:
        return themed(QStringLiteral("view-list-details"), QStyle::SP_FileDialogContentsView);
    case K::Index:
        return themed(QStringLiteral("view-sort-ascending"), QStyle::SP_ArrowUp);
    case K::Trigger:
        return themed(QStringLiteral("code-function"), QStyle::SP_CommandLink);
    case K::Column:
        return themed(QStringLiteral("insert-table"), QStyle::SP_FileIcon);
    }
    return themed(QStringLiteral("text-plain"), QStyle::SP_FileIcon);
}

QIcon folder()
{
    return themed(QStringLiteral("folder"), QStyle::SP_DirIcon);
}

QIcon system()
{
    return themed(QStringLiteral("preferences-system"), QStyle::SP_ComputerIcon);
}

QIcon primaryKey()
{
    return themed(QStringLiteral("password-show-off"), QStyle::SP_DialogApplyButton);
}

QIcon error()
{
    return themed(QStringLiteral("dialog-error"), QStyle::SP_MessageBoxCritical);
}

QIcon refresh()
{
    return themed(QStringLiteral("view-refresh"), QStyle::SP_BrowserReload);
}

QIcon close()
{
    return themed(QStringLiteral("document-close"), QStyle::SP_DialogCloseButton);
}

QIcon run()
{
    return themed(QStringLiteral("media-playback-start"), QStyle::SP_MediaPlay);
}

QIcon runScript()
{
    return themed(QStringLiteral("media-skip-forward"), QStyle::SP_MediaSeekForward);
}

QIcon explain()
{
    return themed(QStringLiteral("view-process-tree"), QStyle::SP_FileDialogListView);
}

QIcon stop()
{
    return themed(QStringLiteral("process-stop"), QStyle::SP_BrowserStop);
}

QIcon diagram()
{
    return themed(QStringLiteral("distribute-graph-directed"), QStyle::SP_FileDialogListView);
}

QIcon commit()
{
    return themed(QStringLiteral("dialog-ok-apply"), QStyle::SP_DialogApplyButton);
}

QIcon rollback()
{
    return themed(QStringLiteral("edit-undo"), QStyle::SP_DialogDiscardButton);
}

QIcon add()
{
    return themed(QStringLiteral("list-add"), QStyle::SP_FileDialogNewFolder);
}

QIcon remove()
{
    return themed(QStringLiteral("list-remove"), QStyle::SP_TrashIcon);
}

QIcon edit()
{
    return themed(QStringLiteral("document-edit"), QStyle::SP_FileDialogContentsView);
}

QIcon exportData()
{
    return themed(QStringLiteral("document-export"), QStyle::SP_DialogSaveButton);
}

QIcon importData()
{
    return themed(QStringLiteral("document-import"), QStyle::SP_DialogOpenButton);
}

QIcon transaction()
{
    static const QIcon icon = cylinder(QColor(0xe0, 0x8a, 0x00));
    return icon;
}

} // namespace sqliteman::Icons
