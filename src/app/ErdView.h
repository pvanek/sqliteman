// SPDX-FileCopyrightText: 2026 Petr Vanek
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "catalog/Erd.h"

#include <QGraphicsView>

class QGraphicsScene;

namespace sqliteman {

class ErdTableItem;

// A diagram of tables and the foreign keys between them. One item per table,
// its columns painted inside it: a box per column would be thousands of
// items in a schema-wide diagram.
class ErdView : public QGraphicsView
{
    Q_OBJECT

public:
    explicit ErdView(QWidget *parent = nullptr);

    void setGraph(const catalog::ErdGraph &graph);
    const catalog::ErdGraph &graph() const { return m_graph; }
    // Where the table the diagram is about sits in the scene, as laid out
    // and wherever it has been dragged to since; empty if there is none.
    QRectF focusBox() const;
    void clear();

    // Everything in sight, however small that makes it.
    void fitDiagram();
    // The table the diagram is about, in the middle and readable. What the
    // diagram opens with: fitting a wide one shrinks it past reading size.
    void focusDiagram();
    void zoomIn() { zoomBy(1.25); }
    void zoomOut() { zoomBy(1 / 1.25); }
    void resetZoom();

    // Saves the diagram as the file name's suffix says: .svg, .png and .pdf
    // as it is drawn now, tables wherever they were dragged to, on white;
    // .dot (or .gv) and .mmd as Graphviz and Mermaid source, which those
    // tools lay out their own way.
    bool saveDiagram(const QString &path, QString *error = nullptr);
    // The suffixes saveDiagram() knows, each with what it means, for a file
    // dialog; the first is the one to offer.
    static QStringList fileFilters();

Q_SIGNALS:
    // A table other than the focus was double-clicked.
    void tableActivated(unsigned int id);

protected:
    void showEvent(QShowEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;
    void changeEvent(QEvent *event) override;

private:
    catalog::ErdMetrics metrics() const;
    void rebuild();
    void zoomBy(qreal factor);
    // What an exported picture covers: everything drawn, with a margin.
    QRectF exportRect() const;
    // Draws the diagram on a light background, unselected, whatever the
    // screen shows: a file is read away from the program's colours.
    void renderForExport(QPainter *painter, const QRectF &target);

    QGraphicsScene *m_scene = nullptr;
    catalog::ErdGraph m_graph;
    QHash<unsigned int, ErdTableItem *> m_tables;
    // Until the user zooms or pans, the view keeps putting the focus table
    // in the middle; afterwards it leaves the view where they left it.
    bool m_userAdjusted = false;
};

} // namespace sqliteman
