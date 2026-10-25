// SPDX-FileCopyrightText: 2026 Petr Vanek
// SPDX-License-Identifier: GPL-3.0-or-later

#include "DiagramFile.h"

#include "Settings.h"
#include "SvgDevice.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QImage>
#include <QMessageBox>
#include <QPainter>
#include <QPdfWriter>
#include <QRegularExpression>
#include <QSaveFile>

#include <cmath>

namespace sqliteman {

namespace {

// A PNG is drawn at twice the size unless that would take more than about
// 256 MB; a diagram of a whole database can be that big.
constexpr qreal PngScale = 2.0;
constexpr qreal MaxPngPixels = 64e6;

QString tr(const char *text)
{
    return QCoreApplication::translate("DiagramFile", text);
}

bool fail(QString *error, const QString &message)
{
    if (error)
        *error = message;
    return false;
}

} // namespace

bool isPictureFile(const QString &path)
{
    const QString suffix = QFileInfo(path).suffix().toLower();
    return suffix == QLatin1String("svg") || suffix == QLatin1String("png")
        || suffix == QLatin1String("pdf");
}

bool savePicture(const QString &path, const QSizeF &size, int dpi, const PictureRenderer &render,
                 QString *error)
{
    const QString suffix = QFileInfo(path).suffix().toLower();
    if (suffix == QLatin1String("png")) {
        const qreal scale
            = std::min(PngScale, std::sqrt(MaxPngPixels / (size.width() * size.height())));
        QImage image((size * scale).toSize(), QImage::Format_ARGB32_Premultiplied);
        if (image.isNull())
            return fail(error, tr("The picture is too big for an image."));
        const int dotsPerMeter = qRound(dpi / 0.0254);
        image.setDotsPerMeterX(dotsPerMeter);
        image.setDotsPerMeterY(dotsPerMeter);
        {
            QPainter painter(&image);
            painter.setRenderHints(QPainter::Antialiasing | QPainter::TextAntialiasing);
            render(&painter, QRectF(QPointF(), QSizeF(image.size())));
        }
        // Printed, it comes out the size it is on the screen.
        image.setDotsPerMeterX(qRound(dotsPerMeter * scale));
        image.setDotsPerMeterY(qRound(dotsPerMeter * scale));
        QSaveFile file(path);
        if (!file.open(QIODevice::WriteOnly) || !image.save(&file, "PNG") || !file.commit())
            return fail(error, file.errorString());
        return true;
    }

    if (suffix == QLatin1String("pdf")) {
        QPdfWriter writer(path);
        writer.setResolution(dpi);
        writer.setPageSize(
            QPageSize(size * 72.0 / dpi, QPageSize::Point, QString(), QPageSize::ExactMatch));
        writer.setPageMargins(QMarginsF());
        writer.setCreator(QStringLiteral("Sqliteman"));
        writer.setTitle(QFileInfo(path).completeBaseName());
        QPainter painter;
        if (!painter.begin(&writer))
            return fail(error, tr("Cannot write to the file."));
        render(&painter, QRectF(QPointF(), size));
        painter.end();
        return true;
    }

    if (suffix == QLatin1String("svg")) {
        SvgDevice device(size, dpi);
        QPainter painter(&device);
        render(&painter, QRectF(QPointF(), size));
        painter.end();
        return saveText(path, device.svg(), error);
    }
    return fail(error, tr("Unknown picture type: choose .svg, .png or .pdf."));
}

bool saveText(const QString &path, const QByteArray &content, QString *error)
{
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly) || file.write(content) != content.size() || !file.commit())
        return fail(error, file.errorString());
    return true;
}

QPalette exportPalette()
{
    QPalette light;
    light.setColor(QPalette::Base, Qt::white);
    light.setColor(QPalette::Text, Qt::black);
    light.setColor(QPalette::Window, QColor(0xef, 0xef, 0xef));
    light.setColor(QPalette::Highlight, QColor(0x30, 0x8c, 0xc6));
    return light;
}

void exportToFile(QWidget *parent, const QString &title, const QString &name, const QString &filter,
                  const std::function<bool(const QString &, QString *)> &save)
{
    QSettings &settings = Settings::instance().store();
    static const QRegularExpression suffixOf(QStringLiteral("\\*(\\.\\w+)"));
    const QString suffix = suffixOf.match(filter).captured(1);
    QString base = name;
    base.replace(QRegularExpression(QStringLiteral("[\\\\/:*?\"<>|]")), QStringLiteral("_"));
    const QString dir = settings.value(QStringLiteral("files/lastDirectory")).toString();
    QString path
        = QFileDialog::getSaveFileName(parent, title, QDir(dir).filePath(base + suffix), filter);
    if (path.isEmpty())
        return;
    if (QFileInfo(path).suffix().isEmpty())
        path += suffix;
    settings.setValue(QStringLiteral("files/lastDirectory"), QFileInfo(path).absolutePath());
    QString error;
    if (!save(path, &error))
        QMessageBox::warning(parent, title, tr("Could not save %1:\n%2").arg(path, error));
}

} // namespace sqliteman
