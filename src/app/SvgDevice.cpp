// SPDX-FileCopyrightText: 2026 Petr Vanek
// SPDX-License-Identifier: GPL-3.0-or-later

#include "SvgDevice.h"

#include <QBuffer>
#include <QImage>
#include <QPaintEngine>
#include <QPainterPath>
#include <QPixmap>
#include <QTextItem>

#include <climits>
#include <cmath>

namespace sqliteman {

namespace {

QString number(qreal value)
{
    QString text = QString::number(value, 'f', 2);
    while (text.endsWith(QLatin1Char('0')))
        text.chop(1);
    if (text.endsWith(QLatin1Char('.')))
        text.chop(1);
    return text == QLatin1String("-0") ? QStringLiteral("0") : text;
}

QString attribute(const char *name, const QString &value)
{
    return QStringLiteral(" %1=\"%2\"").arg(QLatin1String(name), value.toHtmlEscaped());
}

// A color as SVG wants it: the opacity is an attribute of its own.
QString paint(const char *what, const QColor &color)
{
    QString text = attribute(what, color.name(QColor::HexRgb));
    if (color.alpha() < 255)
        text += attribute(QByteArray(what).append("-opacity").constData(), number(color.alphaF()));
    return text;
}

} // namespace

class SvgPaintEngine : public QPaintEngine
{
public:
    explicit SvgPaintEngine(int dpi) : QPaintEngine(AllFeatures), m_dpi(dpi) { }

    const QByteArray &body() const { return m_body; }

    bool begin(QPaintDevice *) override
    {
        m_body.clear();
        return true;
    }
    bool end() override { return true; }
    Type type() const override { return User; }

    void updateState(const QPaintEngineState &changed) override
    {
        const DirtyFlags flags = changed.state();
        if (flags & DirtyPen)
            m_pen = changed.pen();
        if (flags & DirtyBrush)
            m_brush = changed.brush();
        if (flags & DirtyTransform)
            m_transform = changed.transform();
        if (flags & DirtyOpacity)
            m_opacity = changed.opacity();
        // Clipping is left out: the only clip a diagram gets is the page,
        // which the viewBox cuts to anyway.
    }

    void drawPath(const QPainterPath &path) override { writePath(path, true); }

    void drawPolygon(const QPointF *points, int count, PolygonDrawMode mode) override
    {
        if (count <= 0)
            return;
        QPainterPath path;
        path.setFillRule(mode == WindingMode ? Qt::WindingFill : Qt::OddEvenFill);
        path.moveTo(points[0]);
        for (int i = 1; i < count; ++i)
            path.lineTo(points[i]);
        if (mode != PolylineMode)
            path.closeSubpath();
        writePath(path, mode != PolylineMode);
    }

    void drawRects(const QRectF *rects, int count) override
    {
        QPainterPath path;
        for (int i = 0; i < count; ++i)
            path.addRect(rects[i]);
        writePath(path, true);
    }

    void drawLines(const QLineF *lines, int count) override
    {
        QPainterPath path;
        for (int i = 0; i < count; ++i) {
            path.moveTo(lines[i].p1());
            path.lineTo(lines[i].p2());
        }
        writePath(path, false);
    }

    void drawEllipse(const QRectF &rect) override
    {
        QPainterPath path;
        path.addEllipse(rect);
        writePath(path, true);
    }

    void drawTextItem(const QPointF &position, const QTextItem &item) override
    {
        const QString text = item.text();
        if (text.isEmpty())
            return;
        const QFont font = item.font();
        const qreal size = font.pixelSize() > 0 ? font.pixelSize() : font.pointSizeF() * m_dpi / 72;
        QString element = QStringLiteral("<text");
        element += attribute("x", number(position.x()));
        element += attribute("y", number(position.y()));
        element += attribute("font-family", font.family() + QStringLiteral(", sans-serif"));
        element += attribute("font-size", number(size));
        if (font.weight() != QFont::Normal)
            element += attribute("font-weight", QString::number(font.weight()));
        if (font.italic())
            element += attribute("font-style", QStringLiteral("italic"));
        element += paint("fill", m_pen.color());
        // Whatever font the viewer finds, the text takes the width it was
        // laid out for, so it neither spills out of its box nor falls short.
        if (item.width() > 0) {
            element += attribute("textLength", number(item.width()));
            element += attribute("lengthAdjust", QStringLiteral("spacingAndGlyphs"));
        }
        element += common();
        element += QStringLiteral(" xml:space=\"preserve\">") + text.toHtmlEscaped()
            + QStringLiteral("</text>\n");
        m_body += element.toUtf8();
    }

    void drawPixmap(const QRectF &rect, const QPixmap &pixmap, const QRectF &source) override
    {
        drawImage(rect, pixmap.toImage(), source, Qt::AutoColor);
    }

    void drawImage(const QRectF &rect, const QImage &image, const QRectF &source,
                   Qt::ImageConversionFlags) override
    {
        QByteArray png;
        QBuffer buffer(&png);
        buffer.open(QIODevice::WriteOnly);
        image.copy(source.toAlignedRect()).save(&buffer, "PNG");
        QString element = QStringLiteral("<image");
        element += attribute("x", number(rect.x()));
        element += attribute("y", number(rect.y()));
        element += attribute("width", number(rect.width()));
        element += attribute("height", number(rect.height()));
        element += attribute("xlink:href",
                             QStringLiteral("data:image/png;base64,")
                                 + QString::fromLatin1(png.toBase64()));
        element += common() + QStringLiteral("/>\n");
        m_body += element.toUtf8();
    }

private:
    void writePath(const QPainterPath &path, bool fillable)
    {
        if (path.isEmpty())
            return;
        const bool stroked = m_pen.style() != Qt::NoPen && m_pen.brush().style() != Qt::NoBrush;
        const bool filled = fillable && m_brush.style() == Qt::SolidPattern;
        if (!stroked && !filled)
            return;

        QString data;
        for (int i = 0; i < path.elementCount(); ++i) {
            const QPainterPath::Element element = path.elementAt(i);
            switch (element.type) {
            case QPainterPath::MoveToElement:
                data += QStringLiteral("M%1 %2").arg(number(element.x), number(element.y));
                break;
            case QPainterPath::LineToElement:
                data += QStringLiteral("L%1 %2").arg(number(element.x), number(element.y));
                break;
            case QPainterPath::CurveToElement: {
                const QPainterPath::Element second = path.elementAt(i + 1);
                const QPainterPath::Element end = path.elementAt(i + 2);
                data += QStringLiteral("C%1 %2 %3 %4 %5 %6")
                            .arg(number(element.x), number(element.y), number(second.x),
                                 number(second.y), number(end.x), number(end.y));
                i += 2;
                break;
            }
            case QPainterPath::CurveToDataElement:
                break; // Taken with its curve.
            }
        }

        QString element = QStringLiteral("<path") + attribute("d", data);
        element
            += filled ? paint("fill", m_brush.color()) : attribute("fill", QStringLiteral("none"));
        if (filled && path.fillRule() == Qt::OddEvenFill)
            element += attribute("fill-rule", QStringLiteral("evenodd"));
        element += stroked ? stroke() : attribute("stroke", QStringLiteral("none"));
        element += common() + QStringLiteral("/>\n");
        m_body += element.toUtf8();
    }

    QString stroke() const
    {
        QString text = paint("stroke", m_pen.color());
        // A zero width is Qt's cosmetic pen: one pixel whatever the scale.
        const qreal width = m_pen.widthF();
        text += attribute("stroke-width", number(width > 0 ? width : 1));
        if (width <= 0 || m_pen.isCosmetic())
            text += attribute("vector-effect", QStringLiteral("non-scaling-stroke"));
        if (m_pen.style() != Qt::SolidLine) {
            QStringList dashes;
            for (const qreal dash : m_pen.dashPattern())
                dashes << number(dash * std::max<qreal>(width, 1));
            if (!dashes.isEmpty())
                text += attribute("stroke-dasharray", dashes.join(QLatin1Char(' ')));
        }
        switch (m_pen.capStyle()) {
        case Qt::FlatCap:
            text += attribute("stroke-linecap", QStringLiteral("butt"));
            break;
        case Qt::RoundCap:
            text += attribute("stroke-linecap", QStringLiteral("round"));
            break;
        default:
            text += attribute("stroke-linecap", QStringLiteral("square"));
            break;
        }
        switch (m_pen.joinStyle()) {
        case Qt::RoundJoin:
            text += attribute("stroke-linejoin", QStringLiteral("round"));
            break;
        case Qt::BevelJoin:
            text += attribute("stroke-linejoin", QStringLiteral("bevel"));
            break;
        default:
            text += attribute("stroke-linejoin", QStringLiteral("miter"));
            break;
        }
        return text;
    }

    // What every element carries: where it is and how see-through.
    QString common() const
    {
        QString text;
        if (!m_transform.isIdentity()) {
            text += attribute("transform",
                              QStringLiteral("matrix(%1 %2 %3 %4 %5 %6)")
                                  .arg(number(m_transform.m11()), number(m_transform.m12()),
                                       number(m_transform.m21()), number(m_transform.m22()),
                                       number(m_transform.dx()), number(m_transform.dy())));
        }
        if (m_opacity < 1)
            text += attribute("opacity", number(m_opacity));
        return text;
    }

    int m_dpi;
    QByteArray m_body;
    QPen m_pen;
    QBrush m_brush;
    QTransform m_transform;
    qreal m_opacity = 1;
};

SvgDevice::SvgDevice(const QSizeF &size, int dpi)
    : m_size(size), m_dpi(dpi), m_engine(std::make_unique<SvgPaintEngine>(dpi))
{ }

SvgDevice::~SvgDevice() = default;

QByteArray SvgDevice::svg() const
{
    const QString width = number(m_size.width());
    const QString height = number(m_size.height());
    QByteArray document = QStringLiteral("<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
                                         "<svg xmlns=\"http://www.w3.org/2000/svg\" "
                                         "xmlns:xlink=\"http://www.w3.org/1999/xlink\" "
                                         "width=\"%1\" height=\"%2\" viewBox=\"0 0 %1 %2\">\n")
                              .arg(width, height)
                              .toUtf8();
    document += m_engine->body();
    document += "</svg>\n";
    return document;
}

QPaintEngine *SvgDevice::paintEngine() const
{
    return m_engine.get();
}

int SvgDevice::metric(PaintDeviceMetric metric) const
{
    switch (metric) {
    case PdmWidth:
        return int(std::ceil(m_size.width()));
    case PdmHeight:
        return int(std::ceil(m_size.height()));
    case PdmWidthMM:
        return qRound(m_size.width() * 25.4 / m_dpi);
    case PdmHeightMM:
        return qRound(m_size.height() * 25.4 / m_dpi);
    case PdmNumColors:
        return INT_MAX;
    case PdmDepth:
        return 32;
    case PdmDpiX:
    case PdmDpiY:
    case PdmPhysicalDpiX:
    case PdmPhysicalDpiY:
        return m_dpi;
    default:
        return QPaintDevice::metric(metric);
    }
}

} // namespace sqliteman
