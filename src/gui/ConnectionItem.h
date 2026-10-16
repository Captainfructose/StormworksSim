#pragma once

#include <QGraphicsItem>
#include <QPointF>
#include <QPainterPath>
#include "../core/Connection.h"

class NodeGraphItem;

// ─────────────────────────────────────────────────────────────────────────────
// ConnectionItem
//
// Draws a bezier curve wire between the output port of one NodeGraphItem and
// the input port of another.  The curve updates whenever either node moves.
//
// The item holds non-owning pointers to the source and target NodeGraphItems
// so it can always query their current port positions.
// ─────────────────────────────────────────────────────────────────────────────
class ConnectionItem : public QGraphicsItem {
public:
    ConnectionItem(NodeGraphItem* source, int sourcePort,
                   NodeGraphItem* target, int targetPort,
                   QGraphicsItem* parent = nullptr);

    // ── QGraphicsItem interface ───────────────────────────────────────────────
    QRectF boundingRect() const override;
    // Empty shape: wires draw above nodes but must never intercept clicks
    QPainterPath shape() const override { return QPainterPath(); }
    void   paint(QPainter* painter,
                 const QStyleOptionGraphicsItem* option,
                 QWidget* widget = nullptr) override;

    // Accessors for matching against a Connection struct
    int sourceNodeId()  const;
    int targetNodeId()  const;
    int sourcePortIdx() const { return sourcePort_; }
    int targetPortIdx() const { return targetPort_; }

private:
    NodeGraphItem* source_;     // output-side node (non-owning)
    NodeGraphItem* target_;     // input-side node  (non-owning)
    int            sourcePort_; // which output port on source
    int            targetPort_; // which input  port on target
};
