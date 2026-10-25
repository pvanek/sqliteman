// SPDX-FileCopyrightText: 2026 Petr Vanek
// SPDX-License-Identifier: GPL-3.0-or-later

#include "ErdView.h"

#include "DiagramFile.h"
#include "catalog/ErdExport.h"

#include <QEvent>
#include <QFileInfo>
#include <QGraphicsItem>
#include <QGraphicsScene>
#include <QMouseEvent>
#include <QPainter>
#include <QStyleOptionGraphicsItem>
#include <QWheelEvent>

#include <algorithm>
#include <cmath>

namespace sqliteman {

using catalog::ErdColumn;
using catalog::ErdEdge;
using catalog::ErdNode;

namespace {

constexpr qreal MinScale = 0.15;
constexpr qreal MaxScale = 3.0;
// A diagram that opens smaller than this is not worth reading; it is better
// to show the focus table at a sensible size and let the rest be scrolled to.
constexpr qreal MinOpeningScale = 0.7;
// Below this the columns are not readable anyway, so only the name is drawn.
constexpr qreal NamesOnlyDetail = 0.55;

QColor mix(const QColor &a, const QColor &b, float part)
{
    return QColor::fromRgbF(a.redF() * (1 - part) + b.redF() * part,
                            a.greenF() * (1 - part) + b.greenF() * part,
                            a.blueF() * (1 - part) + b.blueF() * part);
}

} // namespace

// One table: header with its name, then a line per column. Nothing about it
// is a child item, so a diagram of 200 tables is 200 items.
class ErdTableItem : public QGraphicsItem
{
public:
    ErdTableItem(const ErdNode &node, const catalog::ErdMetrics &metrics, bool focus)
        : m_node(node), m_metrics(metrics), m_focus(focus)
    {
        m_size = metrics.sizeOf(node);
        setFlags(ItemIsSelectable | ItemIsMovable | ItemSendsGeometryChanges);
        setCacheMode(DeviceCoordinateCache);
        setAcceptHoverEvents(true);
        setZValue(1);
        QStringList tip;
        tip << node.qualifiedName();
        for (const ErdColumn &column : node.columns) {
            tip << QStringLiteral("%1  %2%3")
                       .arg(column.name, column.type,
                            column.primaryKey       ? QStringLiteral("  (primary key)")
                                : column.foreignKey ? QStringLiteral("  (foreign key)")
                                                    : QString());
        }
        if (node.otherColumns > 0)
            tip << ErdView::tr("and %1 columns that are not keys").arg(node.otherColumns);
        setToolTip(tip.join(QLatin1Char('\n')));
    }

    unsigned int id() const { return m_node.id; }
    bool isFocus() const { return m_focus; }
    QRectF boundingRect() const override
    {
        return QRectF(QPointF(), m_size).adjusted(-1, -1, 1, 1);
    }

    QRectF box() const { return QRectF(scenePos(), m_size); }

    // Where an edge meets this table. Sideways it leaves at the column's own
    // row, which is what makes a diagram readable; above or below there is no
    // row to aim at, so it leaves in the middle.
    QPointF sideAnchor(const QString &column, bool right) const
    {
        const QRectF area = box();
        const int row = rowOf(column);
        const qreal y = row < 0
            ? area.center().y()
            : area.top() + m_metrics.headerHeight + m_metrics.rowHeight * (row + 0.5);
        return {right ? area.right() : area.left(), std::min(y, area.bottom() - 2)};
    }

    // Several keys meeting the same edge of a box would all land on the
    // same spot; each gets its own share of the width instead.
    QPointF verticalAnchor(bool bottom, qreal share = 0.5) const
    {
        const QRectF area = box();
        return {area.left() + area.width() * std::clamp(share, 0.15, 0.85),
                bottom ? area.bottom() : area.top()};
    }

protected:
    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *) override
    {
        const QPalette palette = scene()->palette();
        const QRectF box(QPointF(), m_size);
        const QColor base = palette.color(QPalette::Base);
        const QColor text = palette.color(QPalette::Text);
        const QColor line = mix(base, text, 0.35f);
        const bool selected = option->state & QStyle::State_Selected;

        painter->setRenderHint(QPainter::Antialiasing);
        painter->setPen(QPen(selected ? palette.color(QPalette::Highlight) : line,
                             selected || m_focus ? 2 : 1));
        painter->setBrush(base);
        painter->drawRoundedRect(box, 4, 4);

        // The header carries the name, and says which table the diagram is about.
        QRectF header(box.topLeft(), QSizeF(box.width(), m_metrics.headerHeight));
        painter->setBrush(m_focus ? mix(base, palette.color(QPalette::Highlight), 0.35f)
                                  : mix(base, palette.color(QPalette::Window), 0.7f));
        painter->setPen(Qt::NoPen);
        painter->drawRoundedRect(header, 4, 4);
        painter->fillRect(QRectF(header.left(), header.bottom() - 4, header.width(), 4),
                          painter->brush());
        painter->setPen(line);
        painter->drawLine(header.bottomLeft(), header.bottomRight());

        const QFont plain = painter->font();
        QFont bold = plain;
        bold.setBold(true);
        painter->setFont(bold);
        painter->setPen(text);
        const qreal pad = m_metrics.padding;
        painter->drawText(header.adjusted(pad, 0, -pad, 0), Qt::AlignVCenter | Qt::AlignLeft,
                          painter->fontMetrics().elidedText(m_node.name, Qt::ElideRight,
                                                            int(box.width() - 2 * pad)));
        painter->setFont(plain);

        if (option->levelOfDetailFromTransform(painter->worldTransform()) < NamesOnlyDetail)
            return; // Too small to read: the name says enough.

        const int rows = m_metrics.maxRows > 0
            ? std::min(int(m_node.columns.size()), m_metrics.maxRows)
            : int(m_node.columns.size());
        qreal y = box.top() + m_metrics.headerHeight;
        for (int row = 0; row < rows; ++row) {
            const ErdColumn &column = m_node.columns[std::size_t(row)];
            const QRectF area(box.left() + pad, y, box.width() - 2 * pad, m_metrics.rowHeight);
            QFont font = plain;
            font.setBold(column.primaryKey);
            font.setItalic(column.foreignKey);
            painter->setFont(font);
            painter->setPen(text);
            painter->drawText(area, Qt::AlignVCenter | Qt::AlignLeft,
                              painter->fontMetrics().elidedText(column.name, Qt::ElideRight,
                                                                int(area.width() * 0.6)));
            painter->setFont(plain);
            painter->setPen(mix(base, text, 0.6f));
            painter->drawText(area, Qt::AlignVCenter | Qt::AlignRight,
                              painter->fontMetrics().elidedText(column.type, Qt::ElideRight,
                                                                int(area.width() * 0.4)));
            y += m_metrics.rowHeight;
        }
        // Only key columns are drawn, so the box says what it leaves out.
        const int hidden = int(m_node.columns.size()) - rows + m_node.otherColumns;
        if (hidden > 0) {
            QFont italic = plain;
            italic.setItalic(true);
            painter->setFont(italic);
            painter->setPen(mix(base, text, 0.6f));
            painter->drawText(
                QRectF(box.left() + pad, y, box.width() - 2 * pad, m_metrics.rowHeight),
                Qt::AlignVCenter | Qt::AlignLeft,
                hidden == 1 ? ErdView::tr("+ 1 column") : ErdView::tr("+ %1 columns").arg(hidden));
            painter->setFont(plain);
        }
    }

    QVariant itemChange(GraphicsItemChange change, const QVariant &value) override
    {
        if (change == ItemPositionHasChanged && scene())
            scene()->update(); // The edges follow the box.
        return QGraphicsItem::itemChange(change, value);
    }

private:
    int rowOf(const QString &column) const
    {
        const int rows = m_metrics.maxRows > 0
            ? std::min(int(m_node.columns.size()), m_metrics.maxRows)
            : int(m_node.columns.size());
        for (int row = 0; row < rows; ++row) {
            if (m_node.columns[std::size_t(row)].name == column)
                return row;
        }
        return -1;
    }

    ErdNode m_node;
    catalog::ErdMetrics m_metrics;
    bool m_focus = false;
    QSizeF m_size;
};

namespace {

// A foreign key, drawn from the referencing column to the referenced one.
class ErdEdgeItem : public QGraphicsItem
{
public:
    ErdEdgeItem(const ErdEdge &edge, ErdTableItem *from, ErdTableItem *to,
                std::vector<QPointF> bends, qreal fromShare, qreal toShare)
        : m_edge(edge), m_from(from), m_to(to), m_bends(std::move(bends)), m_fromShare(fromShare),
          m_toShare(toShare)
    {
        setZValue(0);
        setToolTip(QStringLiteral("%1\n%2 (%3) → %4 (%5)")
                       .arg(edge.name, from->toolTip().section(QLatin1Char('\n'), 0, 0),
                            edge.fromColumns.join(QStringLiteral(", ")),
                            to->toolTip().section(QLatin1Char('\n'), 0, 0),
                            edge.toColumns.join(QStringLiteral(", "))));
    }

    QRectF boundingRect() const override
    {
        return path().controlPointRect().adjusted(-8, -8, 8, 8);
    }

    // Once a table has been dragged, the channels are no longer where the
    // layout left them, so the edge goes back to a plain curve.
    void forget() { m_moved = true; }
    void setLoopOnTheLeft() { m_loopLeft = true; }

protected:
    void paint(QPainter *painter, const QStyleOptionGraphicsItem *, QWidget *) override
    {
        const QPalette palette = scene()->palette();
        const QColor color
            = mix(palette.color(QPalette::Base), palette.color(QPalette::Text), 0.55f);
        painter->setRenderHint(QPainter::Antialiasing);
        painter->setPen(QPen(color, 1.2));
        painter->setBrush(Qt::NoBrush);
        const QPainterPath line = path();
        painter->drawPath(line);

        // Crow's foot notation, as on any ER diagram: the referencing end
        // may have many rows, so it gets the foot; the referenced end has
        // exactly one when the key cannot be null, zero or one when it can.
        const QPointF many = line.pointAtPercent(0.0);
        const QPointF one = line.pointAtPercent(1.0);
        drawCrowsFoot(painter, many, direction(line, 0.0), color);
        drawOne(painter, one, direction(line, 1.0), color, m_edge.mandatory);
    }

private:
    // Which way the line runs at one of its ends, pointing away from it.
    static qreal direction(const QPainterPath &line, qreal at)
    {
        const QPointF end = line.pointAtPercent(at);
        const QPointF inside = line.pointAtPercent(at == 0.0 ? 0.06 : 0.94);
        return std::atan2(inside.y() - end.y(), inside.x() - end.x());
    }

    // "Many": the foot, three prongs spreading out onto the table's edge,
    // with the circle of "zero or many" behind it.
    static void drawCrowsFoot(QPainter *painter, const QPointF &end, qreal angle,
                              const QColor &color)
    {
        constexpr qreal Length = 12;
        constexpr qreal HalfSpread = 5.5;
        painter->setPen(QPen(color, 1.2));
        painter->setBrush(Qt::NoBrush);
        const QPointF along(std::cos(angle), std::sin(angle));
        const QPointF across(-std::sin(angle), std::cos(angle));
        const QPointF root = end + along * Length;
        painter->drawLine(root, end);
        painter->drawLine(root, end + across * HalfSpread);
        painter->drawLine(root, end - across * HalfSpread);
        painter->drawEllipse(end + along * (Length + 4), 3.5, 3.5);
    }

    // "One": a bar across the line, and a second one for "exactly one".
    static void drawOne(QPainter *painter, const QPointF &end, qreal angle, const QColor &color,
                        bool mandatory)
    {
        constexpr qreal HalfBar = 5;
        painter->setPen(QPen(color, 1.2));
        painter->setBrush(Qt::NoBrush);
        const QPointF along(std::cos(angle), std::sin(angle));
        const QPointF across(-std::sin(angle), std::cos(angle));
        auto bar = [&](qreal distance) {
            const QPointF middle = end + along * distance;
            painter->drawLine(middle - across * HalfBar, middle + across * HalfBar);
        };
        bar(8);
        if (mandatory)
            bar(13); // Two bars: exactly one.
        else
            painter->drawEllipse(end + along * 16, 3.5, 3.5); // Zero or one.
    }

    QPainterPath path() const
    {
        QPainterPath path;
        const QRectF from = m_from->box();
        const QRectF to = m_to->box();
        if (m_edge.isSelfReference()) {
            // A loop out of one side and back in again, on whichever side
            // the other keys left alone.
            const qreal side = m_loopLeft ? from.left() : from.right();
            const qreal reach = m_loopLeft ? -40 : 40;
            const QPointF out(side, from.top() + from.height() * 0.35);
            const QPointF in(side, from.top() + from.height() * 0.65);
            path.moveTo(out);
            path.cubicTo(out + QPointF(reach, -10), in + QPointF(reach, 10), in);
            return path;
        }

        // Tables in between: through the channels the layout kept free, so
        // the line goes round them instead of straight over them.
        if (!m_bends.empty() && !m_moved) {
            const bool downwards = to.center().y() < from.center().y();
            path.moveTo(m_from->verticalAnchor(!downwards, m_fromShare));
            for (const QPointF &bend : m_bends)
                path.lineTo(bend);
            path.lineTo(m_to->verticalAnchor(downwards, m_toShare));
            return path;
        }

        // One above the other: leave through the near horizontal edge, so
        // the line runs straight down the gap instead of around the boxes.
        const bool sideBySide = from.right() < to.left() || to.right() < from.left();
        if (!sideBySide) {
            const bool downwards = to.center().y() > from.center().y();
            const QPointF start = m_from->verticalAnchor(downwards, m_fromShare);
            const QPointF end = m_to->verticalAnchor(!downwards, m_toShare);
            const qreal reach = std::max<qreal>(24, std::abs(end.y() - start.y()) / 3);
            path.moveTo(start);
            path.cubicTo(start + QPointF(0, downwards ? reach : -reach),
                         end + QPointF(0, downwards ? -reach : reach), end);
            return path;
        }

        // Side by side: out of the near side at the key's own row.
        const bool rightwards = to.center().x() > from.center().x();
        const QPointF start = m_from->sideAnchor(m_edge.fromColumns.value(0), rightwards);
        const QPointF end = m_to->sideAnchor(m_edge.toColumns.value(0), !rightwards);
        const qreal reach = std::max<qreal>(30, std::abs(end.x() - start.x()) / 3);
        path.moveTo(start);
        path.cubicTo(start + QPointF(rightwards ? reach : -reach, 0),
                     end + QPointF(rightwards ? -reach : reach, 0), end);
        return path;
    }

    ErdEdge m_edge;
    ErdTableItem *m_from = nullptr;
    ErdTableItem *m_to = nullptr;
    // Where the layout kept a channel free, for an edge that has tables
    // between its ends; empty for a hop to the next layer.
    std::vector<QPointF> m_bends;
    qreal m_fromShare = 0.5; // Where along the box edge each end attaches.
    qreal m_toShare = 0.5;
    bool m_loopLeft = false; // Self-references: which side the loop goes out.
    bool m_moved = false;
};

} // namespace

namespace {

// The frame round a schema's tables in a diagram that spans several.
class ErdClusterItem : public QGraphicsItem
{
public:
    explicit ErdClusterItem(const catalog::ErdCluster &cluster)
        : m_name(cluster.name), m_size(cluster.box.size())
    {
        setZValue(-1); // Behind the tables it holds.
        setPos(cluster.box.topLeft());
        setToolTip(cluster.name);
    }

    QRectF boundingRect() const override { return QRectF(QPointF(), m_size); }

protected:
    void paint(QPainter *painter, const QStyleOptionGraphicsItem *, QWidget *) override
    {
        const QPalette palette = scene()->palette();
        const QColor base = palette.color(QPalette::Base);
        const QColor text = palette.color(QPalette::Text);
        painter->setRenderHint(QPainter::Antialiasing);
        painter->setPen(QPen(mix(base, text, 0.25f), 1, Qt::DashLine));
        painter->setBrush(mix(base, palette.color(QPalette::Window), 0.5f));
        painter->drawRoundedRect(QRectF(QPointF(), m_size), 6, 6);

        QFont bold = painter->font();
        bold.setBold(true);
        painter->setFont(bold);
        painter->setPen(mix(base, text, 0.75f));
        painter->drawText(QRectF(8, 4, m_size.width() - 16, 20), Qt::AlignLeft | Qt::AlignVCenter,
                          m_name);
    }

private:
    QString m_name;
    QSizeF m_size;
};

} // namespace

ErdView::ErdView(QWidget *parent) : QGraphicsView(parent), m_scene(new QGraphicsScene(this))
{
    setScene(m_scene);
    setRenderHint(QPainter::Antialiasing);
    setDragMode(QGraphicsView::ScrollHandDrag);
    setTransformationAnchor(QGraphicsView::AnchorUnderMouse);
    setResizeAnchor(QGraphicsView::AnchorViewCenter);
    setFrameShape(QFrame::NoFrame);
    m_scene->setItemIndexMethod(QGraphicsScene::NoIndex); // Tables are dragged about.
}

void ErdView::setGraph(const catalog::ErdGraph &graph)
{
    m_graph = graph;
    rebuild();
}

void ErdView::clear()
{
    m_graph = {};
    rebuild();
}

catalog::ErdMetrics ErdView::metrics() const
{
    catalog::ErdMetrics metrics;
    const QFontMetricsF fm(font());
    metrics.charWidth = fm.horizontalAdvance(QStringLiteral("n"));
    metrics.rowHeight = fm.height() + 4;
    metrics.headerHeight = fm.height() + 10;
    metrics.minWidth = 24 * metrics.charWidth;
    metrics.maxWidth = 46 * metrics.charWidth;
    metrics.maxRows = 18; // Long tables would make a wall of text.
    return metrics;
}

void ErdView::rebuild()
{
    m_scene->clear(); // Deletes the items; the hash goes with them.
    m_tables.clear();
    if (m_graph.isEmpty()) {
        m_scene->setSceneRect(QRectF());
        return;
    }

    const catalog::ErdMetrics m = metrics();
    const catalog::ErdLayout layout = catalog::layoutFor(m_graph, m);
    for (const catalog::ErdCluster &cluster : layout.clusters)
        m_scene->addItem(new ErdClusterItem(cluster));
    for (const catalog::ErdPlacement &placement : layout.nodes) {
        const catalog::ErdNode *node = m_graph.node(placement.id);
        if (!node)
            continue;
        auto *item = new ErdTableItem(*node, m, node->id == m_graph.focus);
        item->setPos(placement.box.topLeft());
        m_scene->addItem(item);
        m_tables.insert(node->id, item);
    }
    // Each key that touches a table gets its own place along the box edge,
    // in a fixed order, so several of them do not pile up on one point.
    QHash<unsigned int, int> touching;
    for (const catalog::ErdEdge &edge : m_graph.edges) {
        touching[edge.from] += 1;
        if (!edge.isSelfReference())
            touching[edge.to] += 1;
    }
    QHash<unsigned int, int> used;
    auto share = [&](unsigned int id) { return qreal(++used[id]) / (touching.value(id) + 1); };
    for (const catalog::ErdEdge &edge : m_graph.edges) {
        ErdTableItem *from = m_tables.value(edge.from);
        ErdTableItem *to = m_tables.value(edge.to);
        if (!from || !to)
            continue;
        const qreal fromShare = share(edge.from);
        const qreal toShare = edge.isSelfReference() ? fromShare : share(edge.to);
        auto *item
            = new ErdEdgeItem(edge, from, to, layout.route(edge.from, edge.to), fromShare, toShare);
        if (edge.isSelfReference()) {
            // Out of the side the table's other keys use less.
            int right = 0;
            for (const catalog::ErdEdge &other : m_graph.edges) {
                if (other.isSelfReference())
                    continue;
                const unsigned int far = other.from == edge.from ? other.to : other.from;
                if ((other.from == edge.from || other.to == edge.from) && m_tables.contains(far))
                    right += m_tables.value(far)->box().center().x() > from->box().center().x()
                        ? 1
                        : -1;
            }
            if (right > 0)
                item->setLoopOnTheLeft();
        }
        m_scene->addItem(item);
    }

    // From the items, not from the layout: edges curve out beyond the boxes.
    m_scene->setSceneRect(m_scene->itemsBoundingRect().adjusted(-40, -40, 40, 40));
    m_userAdjusted = false;
    focusDiagram();
}

QRectF ErdView::focusBox() const
{
    const ErdTableItem *focus = m_tables.value(m_graph.focus);
    return focus ? focus->box() : QRectF();
}

void ErdView::fitDiagram()
{
    if (m_scene->sceneRect().isNull())
        return;
    fitInView(m_scene->sceneRect(), Qt::KeepAspectRatio);
    // Fitting may blow a small diagram up; it should never look zoomed in.
    if (transform().m11() > 1.0)
        setTransform(QTransform());
    centerOn(m_scene->sceneRect().center());
    m_userAdjusted = true;
}

void ErdView::focusDiagram()
{
    const QRectF scene = m_scene->sceneRect();
    if (scene.isNull() || !viewport() || viewport()->width() <= 1)
        return; // Not on screen yet: showEvent comes back to this.

    const qreal fits
        = std::min(viewport()->width() / scene.width(), viewport()->height() / scene.height());
    // A diagram about one table opens big enough to read, even if that
    // leaves the rest to be scrolled to; one about a schema or a database
    // has no such centre, so it opens showing everything.
    const bool focused = m_tables.contains(m_graph.focus);
    const qreal wanted = std::clamp(fits, focused ? MinOpeningScale : MinScale, 1.0);
    setTransform(QTransform::fromScale(wanted, wanted));

    const QRectF focus = focusBox();
    centerOn(focus.isNull() ? scene.center() : focus.center());
}

void ErdView::resetZoom()
{
    setTransform(QTransform());
    const QRectF focus = focusBox();
    centerOn(focus.isNull() ? m_scene->sceneRect().center() : focus.center());
    m_userAdjusted = true;
}

QStringList ErdView::fileFilters()
{
    return {tr("SVG image (*.svg)"), tr("PNG image (*.png)"), tr("PDF document (*.pdf)"),
            tr("Graphviz (*.dot *.gv)"), tr("Mermaid (*.mmd)")};
}

QRectF ErdView::exportRect() const
{
    return m_scene->itemsBoundingRect().adjusted(-20, -20, 20, 20);
}

void ErdView::renderForExport(QPainter *painter, const QRectF &target)
{
    const QList<QGraphicsItem *> selected = m_scene->selectedItems();
    m_scene->clearSelection();
    m_scene->setPalette(exportPalette());
    // A cached item would go out as a picture of itself, not as lines and
    // text a vector file can keep sharp.
    for (ErdTableItem *table : std::as_const(m_tables))
        table->setCacheMode(QGraphicsItem::NoCache);

    painter->fillRect(target, Qt::white);
    m_scene->render(painter, target, exportRect());

    for (ErdTableItem *table : std::as_const(m_tables))
        table->setCacheMode(QGraphicsItem::DeviceCoordinateCache);
    m_scene->setPalette(QPalette()); // Back to following the application's.
    for (QGraphicsItem *item : selected)
        item->setSelected(true);
}

bool ErdView::saveDiagram(const QString &path, QString *error)
{
    if (m_graph.isEmpty()) {
        if (error)
            *error = tr("There is no diagram to save.");
        return false;
    }
    const QString suffix = QFileInfo(path).suffix().toLower();
    if (isPictureFile(path)) {
        // Fonts are set in points; the diagram was laid out at the screen's
        // resolution, so the file is drawn at the same one.
        return savePicture(
            path, exportRect().size(), logicalDpiY(),
            [this](QPainter *painter, const QRectF &target) { renderForExport(painter, target); },
            error);
    }
    if (suffix == QLatin1String("dot") || suffix == QLatin1String("gv"))
        return saveText(path, catalog::erdToDot(m_graph).toUtf8(), error);
    if (suffix == QLatin1String("mmd"))
        return saveText(path, catalog::erdToMermaid(m_graph).toUtf8(), error);
    if (error)
        *error = tr("Unknown file type: choose .svg, .png, .pdf, .dot or .mmd.");
    return false;
}

void ErdView::showEvent(QShowEvent *event)
{
    QGraphicsView::showEvent(event);
    // The viewport has its real size only now: what was worked out while the
    // tab was hidden would be the wrong scale.
    if (!m_userAdjusted)
        focusDiagram();
}

void ErdView::resizeEvent(QResizeEvent *event)
{
    QGraphicsView::resizeEvent(event);
    if (!m_userAdjusted)
        focusDiagram();
}

void ErdView::zoomBy(qreal factor)
{
    const qreal wanted = std::clamp(transform().m11() * factor, MinScale, MaxScale);
    setTransform(QTransform::fromScale(wanted, wanted));
    m_userAdjusted = true;
}

void ErdView::wheelEvent(QWheelEvent *event)
{
    if (event->modifiers() & Qt::ControlModifier) {
        zoomBy(event->angleDelta().y() > 0 ? 1.15 : 1 / 1.15);
        event->accept();
        return;
    }
    QGraphicsView::wheelEvent(event);
}

void ErdView::mousePressEvent(QMouseEvent *event)
{
    m_userAdjusted = true; // Panning and moving tables are the user's doing.
    QGraphicsView::mousePressEvent(event);
}

void ErdView::mouseDoubleClickEvent(QMouseEvent *event)
{
    if (auto *table = dynamic_cast<ErdTableItem *>(itemAt(event->position().toPoint()))) {
        if (!table->isFocus()) {
            Q_EMIT tableActivated(table->id());
            event->accept();
            return;
        }
    }
    QGraphicsView::mouseDoubleClickEvent(event);
}

void ErdView::changeEvent(QEvent *event)
{
    QGraphicsView::changeEvent(event);
    if (event->type() == QEvent::PaletteChange || event->type() == QEvent::ApplicationPaletteChange)
        m_scene->update();
}

} // namespace sqliteman
