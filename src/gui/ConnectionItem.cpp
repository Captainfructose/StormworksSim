#include "ConnectionItem.h"
#include "NodeGraphItem.h"
#include "../core/Node.h"

#include <QPainter>
#include <QPainterPath>
#include <QStyleOptionGraphicsItem>

// ─────────────────────────────────────────────────────────────────────────────
// Constructor
// ─────────────────────────────────────────────────────────────────────────────
ConnectionItem::ConnectionItem(NodeGraphItem* source, int sourcePort,
                               NodeGraphItem* target, int targetPort,
                               QGraphicsItem* parent)
    : QGraphicsItem(parent),
      source_(source),
      target_(target),
      sourcePort_(sourcePort),
      targetPort_(targetPort)
{
    // Wires draw over nodes (as in Stormworks) so they visibly reach the
    // input circle inside the node's grey panel
    setZValue(1);
}

// ─────────────────────────────────────────────────────────────────────────────
// boundingRect — enclose both endpoints plus some slack for the bezier curve
// ─────────────────────────────────────────────────────────────────────────────
QRectF ConnectionItem::boundingRect() const
{
    if (!source_ || !target_) return QRectF();
    QPointF p1 = source_->outputPortScenePos(sourcePort_);
    QPointF p2 = target_->inputPortScenePos(targetPort_);
    return QRectF(p1, p2).normalized().adjusted(-50, -50, 50, 50);
}

// ─────────────────────────────────────────────────────────────────────────────
// paint — draw a cubic bezier between the two port circle centres
//
// Control points are offset horizontally so the curve has a natural S-shape
// that routes nicely even when nodes are at the same Y position.
// ─────────────────────────────────────────────────────────────────────────────
void ConnectionItem::paint(QPainter* painter,
                            const QStyleOptionGraphicsItem* /*option*/,
                            QWidget* /*widget*/)
{
    if (!source_ || !target_) return;

    QPointF p1 = source_->outputPortScenePos(sourcePort_);
    QPointF p2 = target_->inputPortScenePos(targetPort_);

    // Wire colour: purple for composite, red for bool, green for number
    // Determined by the source node type and output port type
    QColor wireColor = QColor(0x00, 0xe0, 0x50); // default green (number)
    Node* srcNode = source_->node();
    if (srcNode) {
        // Composite wires are always purple regardless of channel type
        if (srcNode->type == NodeType::CompositeInput ||
            srcNode->type == NodeType::CompositePacker ||
            srcNode->type == NodeType::CompositeWriteNumber ||
            srcNode->type == NodeType::CompositeWriteBool ||
            srcNode->type == NodeType::CompositeSwitchbox ||
            srcNode->type == NodeType::NumberToBinary) {
            wireColor = QColor(0x80, 0x00, 0xff); // purple — composite
        } else if (sourcePort_ < (int)srcNode->outputs.size()) {
            if (srcNode->outputs[sourcePort_].type == PortType::Bool)
                wireColor = QColor(0xf0, 0x10, 0x50); // red — bool
        }
    }

    // Stormworks-style wire: a straight line with small direction arrows
    painter->setPen(QPen(wireColor, 2, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    painter->setBrush(Qt::NoBrush);
    painter->drawLine(p1, p2);

    const QLineF line(p1, p2);
    const qreal  len = line.length();
    if (len < 24.0) return;

    constexpr qreal kArrowSpacing = 48.0;
    constexpr qreal kArrowLen     = 7.0;
    constexpr qreal kArrowHalfW   = 3.5;

    const QPointF dir  = (p2 - p1) / len;
    const QPointF perp(-dir.y(), dir.x());

    painter->setPen(Qt::NoPen);
    painter->setBrush(wireColor);
    const int count = std::max(1, (int)(len / kArrowSpacing));
    for (int i = 1; i <= count; ++i) {
        // Centre arrows evenly along the wire
        QPointF c   = p1 + dir * (len * i / (count + 1));
        QPointF tip = c + dir * (kArrowLen * 0.5);
        QPointF base = c - dir * (kArrowLen * 0.5);
        painter->drawPolygon(QPolygonF({tip,
                                        base + perp * kArrowHalfW,
                                        base - perp * kArrowHalfW}));
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Accessors
// ─────────────────────────────────────────────────────────────────────────────
int ConnectionItem::sourceNodeId() const { return source_->nodeId(); }
int ConnectionItem::targetNodeId() const { return target_->nodeId();  }
