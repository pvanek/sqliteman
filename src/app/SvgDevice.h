// SPDX-FileCopyrightText: 2026 Petr Vanek
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QByteArray>
#include <QPaintDevice>
#include <QSizeF>

#include <memory>

namespace sqliteman {

class SvgPaintEngine;

// Something to paint on that comes out as SVG. Qt's own QSvgGenerator would
// mean linking Qt SVG for this alone; what a diagram paints is paths, text
// and the odd pixmap, which is little enough to write out here.
class SvgDevice : public QPaintDevice
{
public:
    // The size is in pixels at the given resolution, which is what fonts
    // set in points are scaled by.
    explicit SvgDevice(const QSizeF &size, int dpi = 96);
    ~SvgDevice() override;

    // The whole document, once the painter has ended.
    QByteArray svg() const;

    QPaintEngine *paintEngine() const override;

protected:
    int metric(PaintDeviceMetric metric) const override;

private:
    QSizeF m_size;
    int m_dpi;
    std::unique_ptr<SvgPaintEngine> m_engine;
};

} // namespace sqliteman
