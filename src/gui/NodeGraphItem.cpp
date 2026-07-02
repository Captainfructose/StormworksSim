#include "NodeGraphItem.h"
#include "../core/nodes/IONodes.h"
#include "../core/nodes/CompositeNodes.h"
#include "../core/nodes/MathNodes.h"
#include "../core/nodes/StatefulNodes.h"
#include <QDebug>

#include <QPainter>
#include <QStyleOptionGraphicsItem>
#include <QGraphicsScene>
#include <QGraphicsSceneMouseEvent>
#include <QFont>
#include <QDialog>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QDoubleSpinBox>
#include <QSpinBox>
#include <QPushButton>
#include <QDialogButtonBox>
#include <QCheckBox>
#include <QComboBox>
#include <QScrollArea>
#include <QFrame>

// ─────────────────────────────────────────────────────────────────────────────
// Constructor
// ─────────────────────────────────────────────────────────────────────────────
NodeGraphItem::NodeGraphItem(Node* node, QGraphicsItem* parent)
    : QGraphicsItem(parent), node_(node)
{
    setFlag(QGraphicsItem::ItemIsMovable);
    setFlag(QGraphicsItem::ItemIsSelectable);
    setFlag(QGraphicsItem::ItemSendsGeometryChanges);
    setPos(node_->posX, node_->posY);
}

// ─────────────────────────────────────────────────────────────────────────────
// totalHeight
//
// Height grows in 40px grid-cell steps:
//   1 port row  -> 2 cells (80px)
//   2 port rows -> 3 cells (120px)
//   3-4 rows    -> 4 cells (160px)
//   5-6 rows    -> 5 cells (200px)
//   ...
// i.e. cells = 2 + floor((rows - 1) / 2), height = cells * kGridCell.
// ─────────────────────────────────────────────────────────────────────────────
int NodeGraphItem::totalHeight() const
{
    int portRows = std::max({(int)node_->inputs.size(), (int)node_->outputs.size(), 1});
    int cells = 2 + (portRows - 1) / 2; // integer division floors automatically
    return cells * kGridCell;
}

// ─────────────────────────────────────────────────────────────────────────────
// Port Y positions
//
// Port rows are spaced evenly within the available body height (total height
// minus the header), regardless of how many grid cells that body spans.
// ─────────────────────────────────────────────────────────────────────────────
qreal NodeGraphItem::inputPortY(int idx) const {
    int portRows = std::max({(int)node_->inputs.size(), (int)node_->outputs.size(), 1});
    qreal bodyH = totalHeight() - kHeaderH;
    qreal rowH  = bodyH / portRows;
    return kHeaderH + idx * rowH + rowH * 0.5;
}
qreal NodeGraphItem::outputPortY(int idx) const {
    int portRows = std::max({(int)node_->inputs.size(), (int)node_->outputs.size(), 1});
    qreal bodyH = totalHeight() - kHeaderH;
    qreal rowH  = bodyH / portRows;
    return kHeaderH + idx * rowH + rowH * 0.5;
}

// ─────────────────────────────────────────────────────────────────────────────
// boundingRect
// ─────────────────────────────────────────────────────────────────────────────
QRectF NodeGraphItem::boundingRect() const
{
    return QRectF(0, 0, kWidth, totalHeight());
}

// ─────────────────────────────────────────────────────────────────────────────
// gearButtonRect — small clickable area in the top-right of the header
// ─────────────────────────────────────────────────────────────────────────────
QRectF NodeGraphItem::gearButtonRect() const
{
    return QRectF(kWidth - kGearSize - 2, (kHeaderH - kGearSize) / 2,
                  kGearSize, kGearSize);
}

// ─────────────────────────────────────────────────────────────────────────────
// hasSettings — returns true for nodes with configurable properties
// ─────────────────────────────────────────────────────────────────────────────
bool NodeGraphItem::hasSettings() const
{
    switch (node_->type) {
        case NodeType::NumberLiteral:
        case NodeType::CompositeReadNumber:
        case NodeType::CompositeReadBool:
        case NodeType::CompositePacker:
        case NodeType::CompositeWriteNumber:
        case NodeType::CompositeWriteBool:
        case NodeType::Clamp:
        case NodeType::EqualTo:
        case NodeType::UpDownCounter:
        case NodeType::TimerTON:
        case NodeType::TimerTOF:
        case NodeType::TimerRTO:
        case NodeType::TimerRTF:
        case NodeType::PIDController:
            return true;
        default:
            return false;
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// headerColor — colour by node category
// ─────────────────────────────────────────────────────────────────────────────
QColor NodeGraphItem::headerColor() const
{
    switch (node_->type) {
        case NodeType::CompositeInput:
        case NodeType::CompositePacker:
        case NodeType::CompositeReadNumber:
        case NodeType::CompositeReadBool:
        case NodeType::CompositeWriteNumber:
        case NodeType::CompositeWriteBool:
            return QColor(0x7a, 0x1a, 0x7a); // purple — composite

        case NodeType::BoolInput:
        case NodeType::BoolOutput:
        case NodeType::NumberInput:
        case NodeType::NumberOutput:
        case NodeType::NumberLiteral:
            return QColor(0x1a, 0x7a, 0x7a); // teal — I/O

        case NodeType::And:
        case NodeType::Or:
        case NodeType::Not:
        case NodeType::Xor:
        case NodeType::Nand:
        case NodeType::Nor:
            return QColor(0x3d, 0x3d, 0xaa); // indigo — logic

        default:
            return QColor(0x8a, 0x5a, 0x00); // amber — math
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// paint
// ─────────────────────────────────────────────────────────────────────────────
void NodeGraphItem::paint(QPainter* painter,
                          const QStyleOptionGraphicsItem* option,
                          QWidget* /*widget*/)
{
    const QRectF bounds(0, 0, kWidth, totalHeight());
    const bool   selected = option->state & QStyle::State_Selected;

    // ── Background ────────────────────────────────────────────────────────────
    painter->setPen(selected ? QPen(QColor(0xff, 0xc0, 0x40), 2)
                              : QPen(QColor(0x55, 0x55, 0x55), 1));
    painter->setBrush(QColor(0x2b, 0x2b, 0x2b));
    painter->drawRoundedRect(bounds, 6, 6);

    // ── Header ────────────────────────────────────────────────────────────────
    QRectF headerRect(0, 0, kWidth, kHeaderH);
    painter->setBrush(headerColor());
    painter->setPen(Qt::NoPen);
    painter->save();
    painter->setClipRect(headerRect);
    painter->drawRoundedRect(bounds, 6, 6);
    painter->restore();

    painter->setPen(QColor(0x44, 0x44, 0x44));
    painter->drawLine(0, kHeaderH, kWidth, kHeaderH);

    // Header text — leave room for gear button if this node has settings
    QFont headerFont = painter->font();
    headerFont.setBold(true);
    headerFont.setPointSize(8);
    painter->setFont(headerFont);
    painter->setPen(Qt::white);
    QRectF textRect = headerRect.adjusted(4, 0, hasSettings() ? -kGearSize - 4 : -4, 0);
    painter->drawText(textRect, Qt::AlignVCenter | Qt::AlignLeft,
                      painter->fontMetrics().elidedText(node_->name, Qt::ElideRight,
                                                        (int)textRect.width()));

    // ── Settings gear button ──────────────────────────────────────────────────
    if (hasSettings()) {
        QRectF gear = gearButtonRect();
        painter->setBrush(QColor(0xff, 0xff, 0xff, 40));
        painter->setPen(QPen(QColor(0xff, 0xff, 0xff, 120), 1));
        painter->drawRoundedRect(gear, 3, 3);
        painter->setPen(Qt::white);
        QFont gearFont = painter->font();
        gearFont.setBold(false);
        gearFont.setPointSize(9);
        painter->setFont(gearFont);
        painter->drawText(gear, Qt::AlignCenter, "⚙");
    }

    // ── Port rows ─────────────────────────────────────────────────────────────
    QFont portFont = painter->font();
    portFont.setBold(false);
    portFont.setPointSize(7);
    painter->setFont(portFont);

    // Determine if this node is a pure source (no inputs shown) or pure sink (no outputs shown)
    bool isSourceNode = (node_->type == NodeType::BoolInput   ||
                         node_->type == NodeType::NumberInput  ||
                         node_->type == NodeType::CompositeInput ||
                         node_->type == NodeType::NumberLiteral);
    bool isSinkNode   = (node_->type == NodeType::BoolOutput  ||
                         node_->type == NodeType::NumberOutput);

    // Helper: get port color based on type, port index, and direction.
    auto portColor = [this](const Port& p, bool isOutput, int portIdx) -> QColor {
        bool isWriteNode = (node_->type == NodeType::CompositeWriteNumber ||
                            node_->type == NodeType::CompositeWriteBool);

        bool isCompositeSide =
            (node_->type == NodeType::CompositeInput ||
             node_->type == NodeType::CompositePacker) ||
            ((node_->type == NodeType::CompositeReadNumber ||
              node_->type == NodeType::CompositeReadBool) && !isOutput) ||
            (isWriteNode && isOutput) ||
            (isWriteNode && !isOutput && portIdx == 0);

        if (isCompositeSide)
            return QColor(0xaa, 0x44, 0xdd);

        return (p.type == PortType::Bool)
               ? QColor(0xdd, 0x44, 0x44)
               : QColor(0x44, 0xdd, 0x44);
    };

    // ── Inner display rectangle ───────────────────────────────────────────────
    // Sits in the centre of the body area, leaving the left/right edges clear
    // for port labels and connection circles. Shows live port states:
    //   • Bool ports: small circle — outlined when false, filled red when true
    //   • Number ports: current value as text
    //   • Logic gate nodes: simplified gate symbol connected to the circles
    //
    // Layout: inner rect occupies the middle third of the node width,
    // vertically spanning the full body (below header).
    constexpr qreal kInnerMarginX = kWidth * 0.32;
    constexpr qreal kInnerMarginY = 4.0;
    QRectF innerRect(kInnerMarginX,
                     kHeaderH + kInnerMarginY,
                     kWidth - kInnerMarginX * 2,
                     totalHeight() - kHeaderH - kInnerMarginY * 2);

    painter->setBrush(QColor(0x22, 0x22, 0x22));
    painter->setPen(QPen(QColor(0x44, 0x44, 0x44), 1));
    painter->drawRoundedRect(innerRect, 3, 3);

    int totalPorts = std::max((int)node_->inputs.size(), (int)node_->outputs.size());
    if (totalPorts == 0) totalPorts = 1;
    qreal rowH = innerRect.height() / totalPorts;

    constexpr qreal kInnerCircleR = 4.0;
    constexpr qreal kInnerPad     = 5.0;

    QFont valFont = painter->font();
    valFont.setPointSize(6);
    painter->setFont(valFont);

    // ── Determine if this is a logic gate node ────────────────────────────────
    bool isLogicGate = (node_->type == NodeType::And  ||
                        node_->type == NodeType::Or   ||
                        node_->type == NodeType::Not  ||
                        node_->type == NodeType::Xor  ||
                        node_->type == NodeType::Nand ||
                        node_->type == NodeType::Nor);

    // ── Input circles (left side of inner rect) ───────────────────────────────
    // For logic gates these connect via lines to the gate body
    for (int i = 0; i < (int)node_->inputs.size(); ++i) {
        const Port& p  = node_->inputs[i];
        qreal cy = innerRect.top() + i * rowH + rowH * 0.5;
        qreal cx = innerRect.left() + kInnerPad + kInnerCircleR;

        if (p.type == PortType::Bool) {
            bool on = p.asBool();
            painter->setBrush(on ? QColor(0xdd, 0x44, 0x44) : Qt::NoBrush);
            painter->setPen(QPen(QColor(0xdd, 0x44, 0x44), 1.5));
            painter->drawEllipse(QPointF(cx, cy), kInnerCircleR, kInnerCircleR);
        } else {
            painter->setPen(QColor(0xaa, 0xaa, 0xaa));
            QRectF valRect(innerRect.left() + 2, cy - rowH * 0.5,
                           innerRect.width() * 0.5 - 2, rowH);
            painter->drawText(valRect, Qt::AlignVCenter | Qt::AlignHCenter,
                              QString::number(p.asNumber(), 'f', 2));
        }
    }

    // ── Output circles (right side of inner rect) ─────────────────────────────
    for (int i = 0; i < (int)node_->outputs.size(); ++i) {
        const Port& p  = node_->outputs[i];
        // For logic gates, always center the single output circle vertically
        qreal cy = isLogicGate ? innerRect.center().y()
                               : innerRect.top() + i * rowH + rowH * 0.5;
        qreal cx = innerRect.right() - kInnerPad - kInnerCircleR;

        if (p.type == PortType::Bool) {
            bool on = p.asBool();
            painter->setBrush(on ? QColor(0xdd, 0x44, 0x44) : Qt::NoBrush);
            painter->setPen(QPen(QColor(0xdd, 0x44, 0x44), 1.5));
            painter->drawEllipse(QPointF(cx, cy), kInnerCircleR, kInnerCircleR);
        } else {
            painter->setPen(QColor(0xaa, 0xaa, 0xaa));
            QRectF valRect(innerRect.left() + innerRect.width() * 0.5,
                           cy - rowH * 0.5,
                           innerRect.width() * 0.5 - 2, rowH);
            painter->drawText(valRect, Qt::AlignVCenter | Qt::AlignHCenter,
                              QString::number(p.asNumber(), 'f', 2));
        }
    }

    // ── Logic gate symbol ─────────────────────────────────────────────────────
    if (isLogicGate) {
        // Gate body fits in the centre of the inner rect, between the circles
        qreal circleEndX   = innerRect.left()  + kInnerPad + kInnerCircleR * 2;
        qreal circleStartX = innerRect.right() - kInnerPad - kInnerCircleR * 2;

        // Gate occupies about 50% of the inner rect width, centred
        qreal gateW  = (circleStartX - circleEndX) * 0.70;
        qreal gateL  = circleEndX  + (circleStartX - circleEndX - gateW) * 0.5;
        qreal gateR  = gateL + gateW;
        qreal gateT  = innerRect.top()    + innerRect.height() * 0.12;
        qreal gateB  = innerRect.bottom() - innerRect.height() * 0.12;
        qreal gateCY = (gateT + gateB) * 0.5;
        qreal gateH  = gateB - gateT;

        QColor gateColor = headerColor().lighter(140);
        painter->setBrush(QColor(0x22, 0x22, 0x22));

        bool hasNegOutput = (node_->type == NodeType::Nand ||
                             node_->type == NodeType::Nor  ||
                             node_->type == NodeType::Not);
        qreal negCircR   = gateH * 0.10;
        qreal gateBodyR  = hasNegOutput ? gateR - negCircR * 2 : gateR;

        // ── Input connection lines: circle right edge → gate left edge ─────────
        painter->setPen(QPen(QColor(0x88, 0x88, 0x88), 1));
        int nInputs = (int)node_->inputs.size();
        for (int i = 0; i < nInputs; ++i) {
            qreal cy    = innerRect.top() + i * rowH + rowH * 0.5;
            qreal fromX = innerRect.left() + kInnerPad + kInnerCircleR * 2;

            // OR/NOR/XOR have a curved left edge — line meets where the curve is
            // at this input's Y position. We connect to gateL for simplicity
            // since the curve is subtle at the scale we draw.
            qreal toX = gateL;
            // For OR/XOR input lines, connect to the curved body's left-most point
            // at this Y (approximated as gateL + a small inset)
            if (node_->type == NodeType::Or  || node_->type == NodeType::Nor ||
                node_->type == NodeType::Xor) {
                // Curve bulges left by ~20% of gateW at the midpoint;
                // at top/bottom it's at gateL so input lines still meet there
                qreal t   = (cy - gateT) / gateH; // 0..1
                qreal bulge = gateW * 0.22 * 4 * t * (1 - t); // parabolic arch
                toX = gateL + bulge;
                // XOR extra line is offset further right
                if (node_->type == NodeType::Xor) toX += gateW * 0.10;
            }
            painter->drawLine(QPointF(fromX, cy), QPointF(toX, cy));
        }

        // ── Output connection line: gate right tip → output circle left edge ───
        qreal outCY   = innerRect.center().y();
        qreal outFromX = hasNegOutput ? gateBodyR + negCircR * 2 : gateBodyR;
        qreal outToX   = innerRect.right() - kInnerPad - kInnerCircleR * 2;
        painter->drawLine(QPointF(outFromX, outCY), QPointF(outToX, outCY));

        // ── Gate body ─────────────────────────────────────────────────────────
        painter->setPen(QPen(gateColor, 1.5));
        QPainterPath path;

        switch (node_->type) {
            case NodeType::And:
            case NodeType::Nand: {
                // D-shape: flat left, semicircle right peaking at gateBodyR
                qreal midY = gateCY;
                path.moveTo(gateL, gateT);
                path.lineTo(gateL, gateB);
                path.cubicTo(gateBodyR, gateB,
                             gateBodyR, gateT,
                             gateL, gateT);
                // Draw the flat left line separately since close won't add it right
                // Actually just draw the arc from top-right through tip to bottom-right
                path = QPainterPath();
                path.moveTo(gateL, gateT);
                path.lineTo(gateL, gateB);
                // Right semicircle passing exactly through gateBodyR at midY
                path.cubicTo(QPointF(gateBodyR, gateB),
                             QPointF(gateBodyR, gateT),
                             QPointF(gateL, gateT));
                // Redraw properly
                path = QPainterPath();
                path.moveTo(gateL, gateT);
                path.lineTo(gateL, gateB);
                qreal cpX = gateBodyR + (gateBodyR - gateL) * 0.2;
                path.cubicTo(QPointF(cpX, gateB),
                             QPointF(cpX, gateT),
                             QPointF(gateL, gateT));
                // That curves away from tip — use proper semicircle approach
                path = QPainterPath();
                qreal midX = gateL + (gateBodyR - gateL) * 0.5;
                path.moveTo(gateL, gateT);
                path.lineTo(gateL, gateB);
                path.cubicTo(QPointF(midX + (gateBodyR-gateL)*0.5, gateB),
                             QPointF(gateBodyR, gateB - gateH*0.25),
                             QPointF(gateBodyR, midY));
                path.cubicTo(QPointF(gateBodyR, gateT + gateH*0.25),
                             QPointF(midX + (gateBodyR-gateL)*0.5, gateT),
                             QPointF(gateL, gateT));
                break;
            }
            case NodeType::Or:
            case NodeType::Nor: {
                qreal bulge = gateW * 0.22;
                path.moveTo(gateL, gateT);
                path.quadTo(gateL + gateW * 0.55, gateT, gateBodyR, gateCY);
                path.quadTo(gateL + gateW * 0.55, gateB, gateL, gateB);
                path.quadTo(gateL + bulge, gateCY, gateL, gateT);
                break;
            }
            case NodeType::Xor: {
                qreal bulge   = gateW * 0.22;
                qreal xOffset = gateW * 0.12;
                // Main OR body shifted right
                path.moveTo(gateL + xOffset, gateT);
                path.quadTo(gateL + gateW * 0.55, gateT, gateBodyR, gateCY);
                path.quadTo(gateL + gateW * 0.55, gateB, gateL + xOffset, gateB);
                path.quadTo(gateL + xOffset + bulge, gateCY, gateL + xOffset, gateT);
                painter->drawPath(path);
                // Extra input-side curve
                QPainterPath xorExtra;
                xorExtra.moveTo(gateL, gateT);
                xorExtra.quadTo(gateL + bulge, gateCY, gateL, gateB);
                painter->drawPath(xorExtra);
                path = QPainterPath();
                break;
            }
            case NodeType::Not: {
                // Triangle pointing right
                path.moveTo(gateL, gateT);
                path.lineTo(gateBodyR, gateCY);
                path.lineTo(gateL, gateB);
                path.closeSubpath();
                break;
            }
            default: break;
        }

        if (!path.isEmpty())
            painter->drawPath(path);

        // ── Negation bubble ───────────────────────────────────────────────────
        if (hasNegOutput) {
            qreal outCY = innerRect.center().y();
            painter->setBrush(QColor(0x22, 0x22, 0x22));
            painter->setPen(QPen(gateColor, 1.5));
            painter->drawEllipse(QPointF(gateBodyR + negCircR, outCY),
                                 negCircR, negCircR);
        }
    }

    // ── Port labels and edge connection circles ───────────────────────────────
    // Input ports (left side) — open circles, hidden on pure source nodes
    if (!isSourceNode) {
        for (int i = 0; i < (int)node_->inputs.size(); ++i) {
            const Port& p  = node_->inputs[i];
            qreal       cy = inputPortY(i);

            painter->setBrush(Qt::NoBrush);
            painter->setPen(QPen(portColor(p, false, i), 1.5));
            painter->drawEllipse(QPointF(kPortRadius + 1, cy), kPortRadius, kPortRadius);

            // Label only — no value text (value is shown in inner rect)
            painter->setPen(QColor(0xcc, 0xcc, 0xcc));
            QRectF labelRect(kPortRadius * 2 + 4, cy - kPortRowH * 0.5,
                             kWidth * 0.32 - kPortRadius * 2 - 6, kPortRowH);
            painter->drawText(labelRect, Qt::AlignVCenter | Qt::AlignLeft, p.label);
        }
    }

    // Output ports (right side) — filled circles, hidden on pure sink nodes
    if (!isSinkNode) {
        for (int i = 0; i < (int)node_->outputs.size(); ++i) {
            const Port& p  = node_->outputs[i];
            qreal       cy = outputPortY(i);
            qreal       cx = kWidth - kPortRadius - 1;

            painter->setBrush(portColor(p, true, i));
            painter->setPen(Qt::NoPen);
            painter->drawEllipse(QPointF(cx, cy), kPortRadius, kPortRadius);

            // Label only — no value text (value is shown in inner rect)
            painter->setPen(QColor(0xcc, 0xcc, 0xcc));
            QRectF labelRect(kWidth * 0.68 + 4, cy - kPortRowH * 0.5,
                             kWidth * 0.32 - kPortRadius * 2 - 6, kPortRowH);
            painter->drawText(labelRect, Qt::AlignVCenter | Qt::AlignLeft, p.label);
        }
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// portScenePos helpers
// ─────────────────────────────────────────────────────────────────────────────
QPointF NodeGraphItem::inputPortScenePos(int idx) const
{
    return mapToScene(QPointF(kPortRadius + 1, inputPortY(idx)));
}

QPointF NodeGraphItem::outputPortScenePos(int idx) const
{
    return mapToScene(QPointF(kWidth - kPortRadius - 1, outputPortY(idx)));
}

// ─────────────────────────────────────────────────────────────────────────────
// inputPortsVisible / outputPortsVisible
// Mirrors the logic in paint() that hides ports on pure source/sink nodes
// ─────────────────────────────────────────────────────────────────────────────
bool NodeGraphItem::inputPortsVisible() const
{
    bool isSourceNode = (node_->type == NodeType::BoolInput   ||
                         node_->type == NodeType::NumberInput  ||
                         node_->type == NodeType::CompositeInput ||
                         node_->type == NodeType::NumberLiteral);
    return !isSourceNode;
}

bool NodeGraphItem::outputPortsVisible() const
{
    bool isSinkNode = (node_->type == NodeType::BoolOutput  ||
                       node_->type == NodeType::NumberOutput);
    return !isSinkNode;
}

// ─────────────────────────────────────────────────────────────────────────────
// inputPortAt / outputPortAt — hit-test a scene position against port circles
//
// Uses a slightly larger radius than the visual circle (kPortRadius + 4) so
// clicking near a port is forgiving rather than requiring pixel precision.
// Returns -1 if no port is within range, or if this node's ports of that
// kind are hidden (pure source/sink nodes).
// ─────────────────────────────────────────────────────────────────────────────
int NodeGraphItem::inputPortAt(const QPointF& scenePos) const
{
    if (!node_) return -1;
    if (!inputPortsVisible()) return -1;

    constexpr qreal kHitRadius = NodeGraphItem::kPortRadius + 4;
    for (int i = 0; i < (int)node_->inputs.size(); ++i) {
        QPointF portPos = inputPortScenePos(i);
        qreal dx = scenePos.x() - portPos.x();
        qreal dy = scenePos.y() - portPos.y();
        if ((dx * dx + dy * dy) <= (kHitRadius * kHitRadius))
            return i;
    }
    return -1;
}

int NodeGraphItem::outputPortAt(const QPointF& scenePos) const
{
    if (!node_) return -1;
    if (!outputPortsVisible()) return -1;

    constexpr qreal kHitRadius = NodeGraphItem::kPortRadius + 4;
    for (int i = 0; i < (int)node_->outputs.size(); ++i) {
        QPointF portPos = outputPortScenePos(i);
        qreal dx = scenePos.x() - portPos.x();
        qreal dy = scenePos.y() - portPos.y();
        if ((dx * dx + dy * dy) <= (kHitRadius * kHitRadius))
            return i;
    }
    return -1;
}

// ─────────────────────────────────────────────────────────────────────────────
// portColorAt — returns the same color used in paint() for a given port
// Used by NodeGraphView to enforce same-color (same-type) connections.
// ─────────────────────────────────────────────────────────────────────────────
QColor NodeGraphItem::portColorAt(int portIndex, bool isOutput) const
{
    // Defensive bounds check — port counts can change at runtime for
    // composite nodes, so never trust the index blindly.
    const std::vector<Port>& ports = isOutput ? node_->outputs : node_->inputs;
    if (portIndex < 0 || portIndex >= (int)ports.size())
        return QColor(0x88, 0x88, 0x88); // fallback grey — should never actually show

    const Port& p = ports[portIndex];

    bool isWriteNode = (node_->type == NodeType::CompositeWriteNumber ||
                        node_->type == NodeType::CompositeWriteBool);

    bool isCompositeSide =
        (node_->type == NodeType::CompositeInput ||
         node_->type == NodeType::CompositePacker) ||
        ((node_->type == NodeType::CompositeReadNumber ||
          node_->type == NodeType::CompositeReadBool) && !isOutput) ||
        (isWriteNode && isOutput) ||
        (isWriteNode && !isOutput && portIndex == 0);

    if (isCompositeSide)
        return QColor(0xaa, 0x44, 0xdd); // purple — composite wire

    return (p.type == PortType::Bool)
           ? QColor(0xdd, 0x44, 0x44)   // red — bool
           : QColor(0x44, 0xdd, 0x44);  // green — number
}

// ─────────────────────────────────────────────────────────────────────────────
// itemChange — sync node position when dragged
// ─────────────────────────────────────────────────────────────────────────────
QVariant NodeGraphItem::itemChange(GraphicsItemChange change,
                                   const QVariant& value)
{
    if (change == ItemPositionHasChanged && node_) {
        QPointF p = value.toPointF();
        node_->posX = (int)p.x();
        node_->posY = (int)p.y();

        // Tell Qt this item's geometry changed so it recalculates this
        // item's own bounding rect — connected ConnectionItems are found
        // and refreshed individually below rather than via a full
        // scene()->update(), which was triggering excessive repaint work
        // (and may have been involved in the crash during heavy dragging).
        prepareGeometryChange();

        if (scene()) {
            // Only invalidate the region this node occupies, not the entire
            // scene — Qt will naturally repaint any ConnectionItem whose
            // boundingRect overlaps this region on the next paint pass.
            scene()->invalidate(sceneBoundingRect().adjusted(-200, -200, 200, 200),
                                QGraphicsScene::ItemLayer);
        }
    }
    return QGraphicsItem::itemChange(change, value);
}

// ─────────────────────────────────────────────────────────────────────────────
// mousePressEvent
//
// Priority order:
//   1. Gear button click → open settings dialog, consume the event.
//   2. Port click (input or output) → ignore the event so it bubbles up to
//      NodeGraphView, which handles wire dragging across the whole scene.
//   3. Otherwise → default QGraphicsItem behaviour (node drag/select).
// ─────────────────────────────────────────────────────────────────────────────
void NodeGraphItem::mousePressEvent(QGraphicsSceneMouseEvent* event)
{
    if (hasSettings() && gearButtonRect().contains(event->pos())) {
        openSettingsDialog();
        event->accept();
        return;
    }

    // Check if the click landed on a port — if so, let NodeGraphView handle
    // the wire-dragging interaction instead of dragging the node itself.
    QPointF scenePos = event->scenePos();
    if (outputPortAt(scenePos) >= 0 || inputPortAt(scenePos) >= 0) {
        event->ignore();
        return;
    }

    QGraphicsItem::mousePressEvent(event);
}

// ─────────────────────────────────────────────────────────────────────────────
// openSettingsDialog
//
// Opens a small modal dialog with settings relevant to this node type.
// Changes take effect immediately on the underlying Node object.
// ─────────────────────────────────────────────────────────────────────────────
void NodeGraphItem::openSettingsDialog()
{
    QDialog dialog;
    dialog.setWindowTitle(QString("Settings — %1").arg(node_->name));
    dialog.setFixedWidth(280);
    dialog.setStyleSheet(
        "QDialog { background: #2a2a2a; color: #ddd; }"
        "QLabel { color: #ccc; }"
        "QPushButton { background: #3a3a3a; color: #ddd; border: 1px solid #555; "
        "  padding: 4px 12px; border-radius: 3px; }"
        "QPushButton:hover { background: #4a4a4a; }"
    );

    auto* layout = new QVBoxLayout(&dialog);
    layout->setSpacing(10);
    layout->setContentsMargins(12, 12, 12, 12);

    // ── Number Literal ────────────────────────────────────────────────────────
    if (node_->type == NodeType::NumberLiteral) {
        auto* lit = static_cast<NumberLiteralNode*>(node_);

        layout->addWidget(new QLabel("Constant Value:"));
        auto* spin = new QDoubleSpinBox;
        spin->setRange(-1e9, 1e9);
        spin->setDecimals(6);
        spin->setValue(lit->constantValue);
        layout->addWidget(spin);

        auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
        layout->addWidget(buttons);

        QObject::connect(buttons, &QDialogButtonBox::accepted, [&]() {
            lit->constantValue = spin->value();
            node_->name = QString("= %1").arg(lit->constantValue);
            update();
            dialog.accept();
        });
        QObject::connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    }

    // ── Composite Read Number ─────────────────────────────────────────────────
    else if (node_->type == NodeType::CompositeReadNumber) {
        auto* rn = static_cast<CompositeReadNumberNode*>(node_);

        layout->addWidget(new QLabel("Channel Index (1–32):"));
        auto* spin = new QSpinBox;
        spin->setRange(1, 32);
        spin->setValue(rn->channelIndex);
        layout->addWidget(spin);

        auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
        layout->addWidget(buttons);

        QObject::connect(buttons, &QDialogButtonBox::accepted, [&]() {
            rn->channelIndex = spin->value();
            node_->name = QString("Comp. Read Num (ch%1)").arg(rn->channelIndex);
            update();
            dialog.accept();
        });
        QObject::connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    }

    // ── Composite Read Bool ───────────────────────────────────────────────────
    else if (node_->type == NodeType::CompositeReadBool) {
        auto* rb = static_cast<CompositeReadBoolNode*>(node_);

        layout->addWidget(new QLabel("Channel Index (1–32):"));
        auto* spin = new QSpinBox;
        spin->setRange(1, 32);
        spin->setValue(rb->channelIndex);
        layout->addWidget(spin);

        auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
        layout->addWidget(buttons);

        QObject::connect(buttons, &QDialogButtonBox::accepted, [&]() {
            rb->channelIndex = spin->value();
            node_->name = QString("Comp. Read Bool (ch%1)").arg(rb->channelIndex);
            update();
            dialog.accept();
        });
        QObject::connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    }

    // ── Composite Packer ──────────────────────────────────────────────────────
    else if (node_->type == NodeType::CompositePacker) {
        auto* packer = static_cast<CompositePackerNode*>(node_);

        layout->addWidget(new QLabel("Channel Count (1–32):"));
        auto* spin = new QSpinBox;
        spin->setRange(1, 32);
        spin->setValue(packer->channelCount);
        layout->addWidget(spin);

        auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
        layout->addWidget(buttons);

        QObject::connect(buttons, &QDialogButtonBox::accepted, [&]() {
            packer->setChannelCount(spin->value());
            update();
            dialog.accept();
        });
        QObject::connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    }

    // ── Composite Write (Number) ──────────────────────────────────────────────
    else if (node_->type == NodeType::CompositeWriteNumber) {
        auto* writeNum = static_cast<CompositeWriteNumberNode*>(node_);

        layout->addWidget(new QLabel("Channel Count (1–32):"));
        auto* spin = new QSpinBox;
        spin->setRange(1, 32);
        spin->setValue(writeNum->channelCount);
        layout->addWidget(spin);

        layout->addWidget(new QLabel("Start Index (1–32):"));
        auto* startSpin = new QSpinBox;
        startSpin->setRange(1, 32);
        startSpin->setValue(writeNum->startIndex);
        layout->addWidget(startSpin);

        auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
        layout->addWidget(buttons);

        QObject::connect(buttons, &QDialogButtonBox::accepted, [&]() {
            writeNum->setChannelCount(spin->value());
            writeNum->setStartIndex(startSpin->value());
            update();
            dialog.accept();
        });
        QObject::connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    }

    // ── Composite Write (Bool) ────────────────────────────────────────────────
    else if (node_->type == NodeType::CompositeWriteBool) {
        auto* writeBool = static_cast<CompositeWriteBoolNode*>(node_);

        layout->addWidget(new QLabel("Channel Count (1–32):"));
        auto* spin = new QSpinBox;
        spin->setRange(1, 32);
        spin->setValue(writeBool->channelCount);
        layout->addWidget(spin);

        layout->addWidget(new QLabel("Start Index (1–32):"));
        auto* startSpin = new QSpinBox;
        startSpin->setRange(1, 32);
        startSpin->setValue(writeBool->startIndex);
        layout->addWidget(startSpin);

        auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
        layout->addWidget(buttons);

        QObject::connect(buttons, &QDialogButtonBox::accepted, [&]() {
            writeBool->setChannelCount(spin->value());
            writeBool->setStartIndex(startSpin->value());
            update();
            dialog.accept();
        });
        QObject::connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    }

    // ── Clamp ──────────────────────────────────────────────────────────────────
    else if (node_->type == NodeType::Clamp) {
        auto* clamp = static_cast<ClampNode*>(node_);

        layout->addWidget(new QLabel("Lower Bound:"));
        auto* minSpin = new QDoubleSpinBox;
        minSpin->setRange(-1e9, 1e9);
        minSpin->setDecimals(3);
        minSpin->setValue(clamp->minBound);
        layout->addWidget(minSpin);

        layout->addWidget(new QLabel("Upper Bound:"));
        auto* maxSpin = new QDoubleSpinBox;
        maxSpin->setRange(-1e9, 1e9);
        maxSpin->setDecimals(3);
        maxSpin->setValue(clamp->maxBound);
        layout->addWidget(maxSpin);

        auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
        layout->addWidget(buttons);

        QObject::connect(buttons, &QDialogButtonBox::accepted, [&]() {
            clamp->minBound = minSpin->value();
            clamp->maxBound = maxSpin->value();
            update();
            dialog.accept();
        });
        QObject::connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    }

    // ── Equal To ───────────────────────────────────────────────────────────────
    else if (node_->type == NodeType::EqualTo) {
        auto* eq = static_cast<EqualToNode*>(node_);

        layout->addWidget(new QLabel("Tolerance:"));
        auto* tolSpin = new QDoubleSpinBox;
        tolSpin->setRange(0.0, 1e6);
        tolSpin->setDecimals(6);
        tolSpin->setSingleStep(0.0001);
        tolSpin->setValue(eq->tolerance);
        layout->addWidget(tolSpin);

        auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
        layout->addWidget(buttons);

        QObject::connect(buttons, &QDialogButtonBox::accepted, [&]() {
            eq->tolerance = tolSpin->value();
            update();
            dialog.accept();
        });
        QObject::connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    }

    // ── Up/Down Counter ────────────────────────────────────────────────────────
    else if (node_->type == NodeType::UpDownCounter) {
        auto* ctr = static_cast<UpDownCounterNode*>(node_);

        layout->addWidget(new QLabel("Increment:"));
        auto* stepSpin = new QDoubleSpinBox;
        stepSpin->setRange(0.001, 1e9);
        stepSpin->setDecimals(3);
        stepSpin->setValue(ctr->step);
        layout->addWidget(stepSpin);

        layout->addWidget(new QLabel("Reset Value:"));
        auto* resetSpin = new QDoubleSpinBox;
        resetSpin->setRange(-1e9, 1e9);
        resetSpin->setDecimals(3);
        resetSpin->setValue(ctr->resetValue);
        layout->addWidget(resetSpin);

        auto* clampCheck = new QCheckBox("Enable Clamping");
        clampCheck->setChecked(ctr->clampEnabled);
        layout->addWidget(clampCheck);

        layout->addWidget(new QLabel("Min:"));
        auto* minSpin = new QDoubleSpinBox;
        minSpin->setRange(-1e9, 1e9);
        minSpin->setDecimals(3);
        minSpin->setValue(ctr->minVal);
        minSpin->setEnabled(ctr->clampEnabled);
        layout->addWidget(minSpin);

        layout->addWidget(new QLabel("Max:"));
        auto* maxSpin = new QDoubleSpinBox;
        maxSpin->setRange(-1e9, 1e9);
        maxSpin->setDecimals(3);
        maxSpin->setValue(ctr->maxVal);
        maxSpin->setEnabled(ctr->clampEnabled);
        layout->addWidget(maxSpin);

        QObject::connect(clampCheck, &QCheckBox::toggled, minSpin, &QDoubleSpinBox::setEnabled);
        QObject::connect(clampCheck, &QCheckBox::toggled, maxSpin, &QDoubleSpinBox::setEnabled);

        auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
        layout->addWidget(buttons);

        QObject::connect(buttons, &QDialogButtonBox::accepted, [&]() {
            ctr->step         = stepSpin->value();
            ctr->resetValue   = resetSpin->value();
            ctr->clampEnabled = clampCheck->isChecked();
            ctr->minVal       = minSpin->value();
            ctr->maxVal       = maxSpin->value();
            update();
            dialog.accept();
        });
        QObject::connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    }

    // ── Timers (TON / TOF / RTO / RTF) ───────────────────────────────────────
    // All four timers share the same single setting: whether the Duration
    // input is interpreted as seconds or ticks.
    else if (node_->type == NodeType::TimerTON || node_->type == NodeType::TimerTOF ||
             node_->type == NodeType::TimerRTO || node_->type == NodeType::TimerRTF) {

        // Read current useTicks value from whichever timer type this is
        auto getUseTicks = [this]() -> bool& {
            if (node_->type == NodeType::TimerTON)
                return static_cast<TimerTONNode*>(node_)->useTicks;
            if (node_->type == NodeType::TimerTOF)
                return static_cast<TimerTOFNode*>(node_)->useTicks;
            if (node_->type == NodeType::TimerRTO)
                return static_cast<TimerRTONode*>(node_)->useTicks;
            return static_cast<TimerRTFNode*>(node_)->useTicks;
        };

        layout->addWidget(new QLabel("Duration Unit:"));
        auto* combo = new QComboBox;
        combo->addItem("Seconds");
        combo->addItem("Ticks");
        combo->setCurrentIndex(getUseTicks() ? 1 : 0);
        layout->addWidget(combo);

        auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
        layout->addWidget(buttons);

        QObject::connect(buttons, &QDialogButtonBox::accepted, [&, getUseTicks]() {
            getUseTicks() = (combo->currentIndex() == 1);
            update();
            dialog.accept();
        });
        QObject::connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    }

    // ── PID Controller ─────────────────────────────────────────────────────────
    else if (node_->type == NodeType::PIDController) {
        auto* pid = static_cast<PIDControllerNode*>(node_);

        layout->addWidget(new QLabel("Proportional Gain (Kp):"));
        auto* kpSpin = new QDoubleSpinBox;
        kpSpin->setRange(-1e9, 1e9);
        kpSpin->setDecimals(6);
        kpSpin->setSingleStep(0.1);
        kpSpin->setValue(pid->kp);
        layout->addWidget(kpSpin);

        layout->addWidget(new QLabel("Integral Gain (Ki):"));
        auto* kiSpin = new QDoubleSpinBox;
        kiSpin->setRange(-1e9, 1e9);
        kiSpin->setDecimals(6);
        kiSpin->setSingleStep(0.01);
        kiSpin->setValue(pid->ki);
        layout->addWidget(kiSpin);

        layout->addWidget(new QLabel("Derivative Gain (Kd):"));
        auto* kdSpin = new QDoubleSpinBox;
        kdSpin->setRange(-1e9, 1e9);
        kdSpin->setDecimals(6);
        kdSpin->setSingleStep(0.01);
        kdSpin->setValue(pid->kd);
        layout->addWidget(kdSpin);

        auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
        layout->addWidget(buttons);

        QObject::connect(buttons, &QDialogButtonBox::accepted, [&]() {
            pid->kp = kpSpin->value();
            pid->ki = kiSpin->value();
            pid->kd = kdSpin->value();
            update();
            dialog.accept();
        });
        QObject::connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    }

    dialog.exec();
}
