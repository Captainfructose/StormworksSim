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
    // Wires sit behind nodes in the Z-order
    setZValue(-1);
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

    // Horizontal offset for control points — scales with the distance between nodes
    qreal dx = std::abs(p2.x() - p1.x()) * 0.5;
    dx = std::max(dx, 40.0); // minimum curve breadth

    QPointF ctrl1(p1.x() + dx, p1.y());
    QPointF ctrl2(p2.x() - dx, p2.y());

    QPainterPath path;
    path.moveTo(p1);
    path.cubicTo(ctrl1, ctrl2, p2);

    // Wire colour: purple for composite, red for bool, green for number
    // Determined by the source node type and output port type
    QColor wireColor = QColor(0x44, 0xdd, 0x44); // default green (number)
    Node* srcNode = source_->node();
    if (srcNode) {
        // Composite wires are always purple regardless of channel type
        if (srcNode->type == NodeType::CompositeInput ||
            srcNode->type == NodeType::CompositePacker ||
            srcNode->type == NodeType::CompositeWriteNumber ||
            srcNode->type == NodeType::CompositeWriteBool) {
            wireColor = QColor(0xaa, 0x44, 0xdd); // purple — composite
        } else if (sourcePort_ < (int)srcNode->outputs.size()) {
            if (srcNode->outputs[sourcePort_].type == PortType::Bool)
                wireColor = QColor(0xdd, 0x44, 0x44); // red — bool
        }
    }

    painter->setPen(QPen(wireColor, 2, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    painter->setBrush(Qt::NoBrush);
    painter->drawPath(path);
}

// ─────────────────────────────────────────────────────────────────────────────
// Accessors
// ─────────────────────────────────────────────────────────────────────────────
int ConnectionItem::sourceNodeId() const { return source_->nodeId(); }
int ConnectionItem::targetNodeId() const { return target_->nodeId();  }
