#include "NodeGraphItem.h"
#include "../core/nodes/IONodes.h"
#include "../core/nodes/CompositeNodes.h"
#include "../core/nodes/MathNodes.h"
#include "../core/nodes/StatefulNodes.h"
#include "../core/nodes/FunctionNodes.h"
#include <QDebug>

#include <QPainter>
#include <QCoreApplication>
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
#include <QLineEdit>
#include <QCheckBox>
#include <QRadioButton>
#include <QPlainTextEdit>
#include <QButtonGroup>
#include <algorithm>
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
// innerRect — grey display panel, spanning the body below the header
// inputPortLocalPos — input port circle, inside the grey panel's left side
// ─────────────────────────────────────────────────────────────────────────────
QRectF NodeGraphItem::innerRect() const
{
    return QRectF(kInnerMarginX,
                  kHeaderH + kInnerMarginY,
                  kWidth - kInnerMarginX * 2,
                  totalHeight() - kHeaderH - kInnerMarginY * 2);
}

QPointF NodeGraphItem::inputPortLocalPos(int idx) const
{
    // Input ports sit on the node's left edge; the grey panel is display-only.
    return QPointF(kPortRadius + 1, inputPortY(idx));
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
        case NodeType::BoolInput:
        case NodeType::BoolOutput:
        case NodeType::NumberInput:
        case NodeType::NumberOutput:
        case NodeType::CompositeInput:
        case NodeType::CompositeOutput:
        case NodeType::Capacitor:
        case NodeType::LuaScript:
        case NodeType::Blinker:
        case NodeType::Threshold:
        case NodeType::PropertyDropdown:
        case NodeType::PropertyNumber:
        case NodeType::PropertySlider:
        case NodeType::PropertyToggle:
        case NodeType::PropertyText:
        case NodeType::TooltipNumber:
        case NodeType::TooltipOnOff:
        case NodeType::Function1:
        case NodeType::Function3:
        case NodeType::Function8:
        case NodeType::FunctionBool4:
        case NodeType::FunctionBool8:
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
    // Stormworks-style: flat white block with a thin grey outline
    constexpr qreal kCornerR = 6.0;
    const QPen outlinePen = selected ? QPen(QColor(0xff, 0xa0, 0x20), 2)
                                     : QPen(QColor(0x9a, 0x9a, 0x9a), 1);
    painter->setPen(Qt::NoPen);
    painter->setBrush(QColor(0xf4, 0xf4, 0xf4));
    painter->drawRoundedRect(bounds, kCornerR, kCornerR);

    // SR Latch / JK Flip-Flop use the in-game look: plain white block, small
    // grey icon with the letters at top left, no header strip or grey panel.
    const bool gameStyle = (node_->type == NodeType::SRLatch ||
                            node_->type == NodeType::JKFlipFlop);

    // ── Header ────────────────────────────────────────────────────────────────
    // Clipped to the rounded body so the header follows the top corners
    QRectF headerRect(0, 0, kWidth, kHeaderH);
    if (!gameStyle) {
        painter->save();
        QPainterPath bodyPath;
        bodyPath.addRoundedRect(bounds, kCornerR, kCornerR);
        painter->setClipPath(bodyPath);
        painter->setBrush(QColor(0xdc, 0xdc, 0xdc));
        painter->drawRect(headerRect);
        painter->restore();

        painter->setPen(QColor(0x9a, 0x9a, 0x9a));
        painter->drawLine(0, kHeaderH, kWidth, kHeaderH);
    }

    // Outline last so the header fill doesn't cover it
    painter->setPen(outlinePen);
    painter->setBrush(Qt::NoBrush);
    painter->drawRoundedRect(bounds, kCornerR, kCornerR);

    if (gameStyle) {
        // Icon square with the node's letters
        const QRectF icon(8, 8, 20, 20);
        painter->setPen(Qt::NoPen);
        painter->setBrush(QColor(0x80, 0x80, 0x80));
        painter->drawRoundedRect(icon, 2, 2);

        QFont iconFont = painter->font();
        iconFont.setBold(true);
        iconFont.setPointSize(8);
        painter->setFont(iconFont);
        painter->setPen(Qt::white);
        painter->drawText(icon, Qt::AlignCenter,
                          node_->type == NodeType::JKFlipFlop ? "JK" : "SR");

        // Small title beside the icon ("JK Flip-Flop" -> "JK Flip Flop")
        QFont titleFont = painter->font();
        titleFont.setBold(false);
        titleFont.setPointSize(7);
        painter->setFont(titleFont);
        painter->setPen(QColor(0x33, 0x33, 0x33));
        QString title = node_->name;
        title.replace('-', ' ');
        painter->drawText(QRectF(icon.right() + 5, icon.top(), kWidth - icon.right() - 10,
                                 icon.height()),
                          Qt::AlignVCenter | Qt::AlignLeft, title);

        // Ports — inputs are rings on the left edge, outputs filled discs on the
        // right edge, all bool (red), each with its label. No state panel.
        const QColor red(0xf0, 0x10, 0x50);
        const QColor labelColor(0x33, 0x33, 0x33);
        for (int i = 0; i < (int)node_->inputs.size(); ++i) {
            const QPointF c = inputPortLocalPos(i);
            painter->setBrush(Qt::NoBrush);
            painter->setPen(QPen(red, 2.5));
            painter->drawEllipse(c, kPortRadius - 1, kPortRadius - 1);

            painter->setPen(labelColor);
            painter->drawText(QRectF(kPortRadius * 2 + 4, c.y() - kPortRowH * 0.5,
                                     kWidth * 0.4, kPortRowH),
                              Qt::AlignVCenter | Qt::AlignLeft, node_->inputs[i].label);
        }
        for (int i = 0; i < (int)node_->outputs.size(); ++i) {
            const qreal cy = outputPortY(i);
            painter->setBrush(red);
            painter->setPen(QPen(red.darker(220), 1.5));
            painter->drawEllipse(QPointF(kWidth - kPortRadius - 1, cy),
                                 kPortRadius, kPortRadius);

            painter->setPen(labelColor);
            painter->drawText(QRectF(kWidth * 0.6 - kPortRadius * 2 - 4, cy - kPortRowH * 0.5,
                                     kWidth * 0.4, kPortRowH),
                              Qt::AlignVCenter | Qt::AlignRight, node_->outputs[i].label);
        }
        return;
    }

    // Header text — leave room for gear button if this node has settings
    QFont headerFont = painter->font();
    headerFont.setBold(true);
    headerFont.setPointSize(8);
    painter->setFont(headerFont);
    painter->setPen(QColor(0x33, 0x33, 0x33));
    QRectF textRect = headerRect.adjusted(4, 0, hasSettings() ? -kGearSize - 4 : -4, 0);
    painter->drawText(textRect, Qt::AlignVCenter | Qt::AlignLeft,
                      painter->fontMetrics().elidedText(
                          node_->label.isEmpty() ? node_->name : node_->label,
                          Qt::ElideRight, (int)textRect.width()));

    // ── Settings gear button ──────────────────────────────────────────────────
    if (hasSettings()) {
        QRectF gear = gearButtonRect();
        painter->setBrush(QColor(0x00, 0x00, 0x00, 20));
        painter->setPen(QPen(QColor(0x00, 0x00, 0x00, 90), 1));
        painter->drawRoundedRect(gear, 3, 3);
        painter->setPen(QColor(0x33, 0x33, 0x33));
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
                         node_->type == NodeType::NumberLiteral ||
                         node_->type == NodeType::PropertyDropdown ||
                         node_->type == NodeType::PropertyNumber ||
                         node_->type == NodeType::PropertySlider ||
                         node_->type == NodeType::PropertyToggle);
    bool isSinkNode  = (node_->type == NodeType::BoolOutput  ||
                         node_->type == NodeType::NumberOutput ||
                         node_->type == NodeType::CompositeOutput);

    // Helper: get port color based on type, port index, and direction.
    auto portColor = [](const Port& p, bool /*isOutput*/, int /*portIdx*/) -> QColor {
        switch (p.type) {
            case PortType::Composite: return QColor(0x80, 0x00, 0xff); // purple
            case PortType::Bool:      return QColor(0xf0, 0x10, 0x50); // red
            default:                  return QColor(0x00, 0xe0, 0x50); // green
        }
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
    const QRectF innerRect = this->innerRect();

    // Grey "icon" panel, like the grey squares on Stormworks nodes
    painter->setBrush(QColor(0x80, 0x80, 0x80));
    painter->setPen(Qt::NoPen);
    painter->drawRoundedRect(innerRect, 3, 3);

    int totalPorts = std::max((int)node_->inputs.size(), (int)node_->outputs.size());
    if (totalPorts == 0) totalPorts = 1;
    qreal rowH = innerRect.height() / totalPorts;

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
        if (p.type == PortType::Composite) continue; // no single value to show
        // These are display-only state indicators — the real input ports are the
        // rings on the node's left edge. Source/sink nodes show one centred
        // indicator (circle or value).
        const bool stateOnly = isSourceNode || isSinkNode;
        const qreal cy = innerRect.top() + i * rowH + rowH * 0.5;
        const qreal cx = stateOnly ? innerRect.center().x()
                                   : innerRect.left() + kInnerPad + kInnerCircleR;

        if (p.type == PortType::Bool) {
            bool on = p.asBool();
            const QColor red(0xf0, 0x10, 0x50);
            // A ring, filled while the signal is on. All grey-panel circles share
            // this size/style. (QBrush(...) is explicit: a bare
            // `on ? col : Qt::NoBrush` would convert NoBrush to black.)
            painter->setBrush(on ? QBrush(red) : QBrush(Qt::NoBrush));
            painter->setPen(QPen(red, kInnerRingW));
            painter->drawEllipse(QPointF(cx, cy), kInnerRingR, kInnerRingR);
        } else {
            painter->setPen(Qt::white);
            QRectF valRect = stateOnly
                ? QRectF(innerRect.left(), cy - rowH * 0.5, innerRect.width(), rowH)
                : QRectF(innerRect.left() + 2, cy - rowH * 0.5,
                         innerRect.width() * 0.5 - 2, rowH);
            painter->drawText(valRect, Qt::AlignVCenter | Qt::AlignHCenter,
                              QString::number(p.asNumber(), 'f', 2));
        }
    }

    // ── Constant Number: show the configured value in the grey panel ──────────
    if (node_->type == NodeType::NumberLiteral) {
        auto* lit = static_cast<NumberLiteralNode*>(node_);
        painter->setPen(Qt::white);
        painter->drawText(innerRect, Qt::AlignCenter,
                          QString::number(lit->constantValue, 'g', 8));
    }

    // ── Property nodes: property name above the current value ─────────────────
    // The name is shown smaller and dimmer on the top line; the value (or
    // ON/OFF for a toggle) sits below it. Property Text has only a name.
    {
        QString name, value;
        bool    isProperty = true;
        switch (node_->type) {
            case NodeType::PropertyDropdown: {
                auto* dd = static_cast<PropertyDropdownNode*>(node_);
                name  = dd->propertyName;
                value = QString::number(dd->selectedValue(), 'g', 8);
                break;
            }
            case NodeType::PropertyNumber: {
                auto* pn = static_cast<PropertyNumberNode*>(node_);
                name  = pn->propertyName;
                value = QString::number(pn->value, 'g', 8);
                break;
            }
            case NodeType::PropertySlider: {
                auto* ps = static_cast<PropertySliderNode*>(node_);
                name  = ps->propertyName;
                value = QString::number(ps->value, 'g', 8);
                break;
            }
            case NodeType::PropertyToggle: {
                auto* pt = static_cast<PropertyToggleNode*>(node_);
                name  = pt->propertyName;
                value = pt->value ? "ON" : "OFF";
                break;
            }
            case NodeType::PropertyText:
                name = static_cast<PropertyTextNode*>(node_)->propertyName;
                break;
            default:
                isProperty = false;
                break;
        }

        if (isProperty) {
            const QFontMetrics fm(painter->font());
            const QRectF nameRect(innerRect.left() + 3, innerRect.top() + 2,
                                  innerRect.width() - 6, innerRect.height() * 0.5 - 2);
            const QRectF valueRect(innerRect.left() + 3, innerRect.center().y(),
                                   innerRect.width() - 6, innerRect.height() * 0.5 - 2);

            if (value.isEmpty()) {
                // Name only: use the whole panel
                painter->setPen(Qt::white);
                painter->drawText(innerRect.adjusted(3, 0, -3, 0), Qt::AlignCenter,
                                  fm.elidedText(name, Qt::ElideRight, (int)innerRect.width() - 6));
            } else {
                painter->setPen(QColor(0xdd, 0xdd, 0xdd));
                painter->drawText(nameRect, Qt::AlignCenter,
                                  fm.elidedText(name, Qt::ElideRight, (int)nameRect.width()));
                painter->setPen(Qt::white);
                painter->drawText(valueRect, Qt::AlignCenter,
                                  fm.elidedText(value, Qt::ElideRight, (int)valueRect.width()));
            }
        }
    }

    // ── Output circles (right side of inner rect) ─────────────────────────────
    // Source/sink nodes already show their value above, so skip the duplicate.
    for (int i = 0; i < (int)node_->outputs.size() && !isSourceNode && !isSinkNode; ++i) {
        const Port& p  = node_->outputs[i];
        if (p.type == PortType::Composite) continue; // no single value to show
        // For logic gates, always center the single output circle vertically
        qreal cy = isLogicGate ? innerRect.center().y()
                               : innerRect.top() + i * rowH + rowH * 0.5;
        qreal cx = innerRect.right() - kInnerPad - kInnerCircleR;

        if (p.type == PortType::Bool) {
            bool on = p.asBool();
            const QColor red(0xf0, 0x10, 0x50);
            painter->setBrush(on ? QBrush(red) : QBrush(Qt::NoBrush));
            painter->setPen(QPen(red, kInnerRingW));
            painter->drawEllipse(QPointF(cx, cy), kInnerRingR, kInnerRingR);
        } else {
            painter->setPen(Qt::white);
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

        QColor gateColor = Qt::white;
        painter->setBrush(QColor(0x80, 0x80, 0x80));

        bool hasNegOutput = (node_->type == NodeType::Nand ||
                             node_->type == NodeType::Nor  ||
                             node_->type == NodeType::Not);
        qreal negCircR   = gateH * 0.10;
        qreal gateBodyR  = hasNegOutput ? gateR - negCircR * 2 : gateR;

        // ── Input connection lines: circle right edge → gate left edge ─────────
        painter->setPen(QPen(Qt::white, kGateLineW));
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
        painter->setPen(QPen(gateColor, kGateLineW));
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
            painter->setBrush(QColor(0x80, 0x80, 0x80));
            painter->setPen(QPen(gateColor, kGateLineW));
            painter->drawEllipse(QPointF(gateBodyR + negCircR, outCY),
                                 negCircR, negCircR);
        }
    }

    // ── Port labels and edge connection circles ───────────────────────────────
    // Input ports (left edge) — rings, hidden on pure source nodes
    if (!isSourceNode) {
        for (int i = 0; i < (int)node_->inputs.size(); ++i) {
            const Port& p  = node_->inputs[i];
            qreal       cy = inputPortLocalPos(i).y();
            const qreal labelX = kPortRadius * 2 + 4;

            // Input ports are rings (outputs are filled discs)
            painter->setBrush(Qt::NoBrush);
            painter->setPen(QPen(portColor(p, false, i), 2.5));
            painter->drawEllipse(inputPortLocalPos(i), kPortRadius - 1, kPortRadius - 1);

            painter->setPen(QColor(0x33, 0x33, 0x33));
            QRectF labelRect(labelX, cy - kPortRowH * 0.5,
                             kInnerMarginX - labelX - 2, kPortRowH);
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
            painter->setPen(QPen(portColor(p, true, i).darker(220), 1.5));
            painter->drawEllipse(QPointF(cx, cy), kPortRadius, kPortRadius);

            // Label only — no value text (value is shown in inner rect)
            painter->setPen(QColor(0x33, 0x33, 0x33));
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
    return mapToScene(inputPortLocalPos(idx));
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
                         node_->type == NodeType::NumberLiteral ||
                         node_->type == NodeType::PropertyDropdown ||
                         node_->type == NodeType::PropertyNumber ||
                         node_->type == NodeType::PropertySlider ||
                         node_->type == NodeType::PropertyToggle);
    return !isSourceNode;
}

bool NodeGraphItem::outputPortsVisible() const
{
    bool isSinkNode = (node_->type == NodeType::BoolOutput  ||
                       node_->type == NodeType::NumberOutput ||
                       node_->type == NodeType::CompositeOutput);
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

    switch (p.type) {
        case PortType::Composite: return QColor(0x80, 0x00, 0xff); // purple — composite wire
        case PortType::Bool:      return QColor(0xf0, 0x10, 0x50); // red — bool
        default:                  return QColor(0x00, 0xe0, 0x50); // green — number
    }
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
// Opens a small modal dialog with settings relevant with this node type.
// Changes take effect immediately on the underlying Node object.
// ─────────────────────────────────────────────────────────────────────────────
void NodeGraphItem::openSettingsDialog()
{
    QDialog dialog;
    dialog.setWindowTitle(QString("Settings — %1").arg(node_->name));
    dialog.setMinimumWidth(400);
    dialog.setStyleSheet(
        "QDialog { background: #2a2a2a; color: #ddd; }"
        "QLabel { color: #ccc; }"
        "QPushButton { background: #3a3a3a; color: #ddd; border: 1px solid #555; "
        "padding: 4px 12px; border-radius: 3px; }"
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

    // ── Property Dropdown ──────────────────────────────────────────────────────
    // Name, plus a list of options (label + value) with one selected.
    // Edits a local copy and writes it back on OK.
    else if (node_->type == NodeType::PropertyDropdown) {
        auto* dd = static_cast<PropertyDropdownNode*>(node_);
        dialog.setMinimumWidth(420);

        layout->addWidget(new QLabel("Property Name:"));
        auto* nameEdit = new QLineEdit(dd->propertyName);
        layout->addWidget(nameEdit);

        layout->addWidget(new QLabel("Options (select the active one):"));

        struct Row {
            QRadioButton*   radio;
            QLineEdit*      label;
            QDoubleSpinBox* value;
        };
        std::vector<Row> rows;
        auto* rowsHolder = new QWidget;
        auto* rowsLayout = new QVBoxLayout(rowsHolder);
        rowsLayout->setContentsMargins(0, 0, 0, 0);
        auto* radioGroup = new QButtonGroup(&dialog);
        layout->addWidget(rowsHolder);

        // Adds one editable option row; removal hides+drops it from `rows`
        auto addRow = [&](const QString& label, double value, bool selected) {
            auto* rowWidget = new QWidget;
            auto* rowLayout = new QHBoxLayout(rowWidget);
            rowLayout->setContentsMargins(0, 0, 0, 0);

            auto* radio = new QRadioButton;
            radio->setChecked(selected);
            radioGroup->addButton(radio);
            auto* labelEdit = new QLineEdit(label);
            auto* valueSpin = new QDoubleSpinBox;
            valueSpin->setRange(-1e9, 1e9);
            valueSpin->setDecimals(4);
            valueSpin->setValue(value);
            auto* removeBtn = new QPushButton("✕");
            removeBtn->setFixedWidth(28);

            rowLayout->addWidget(radio);
            rowLayout->addWidget(labelEdit, 1);
            rowLayout->addWidget(valueSpin);
            rowLayout->addWidget(removeBtn);
            rowsLayout->addWidget(rowWidget);
            rows.push_back({ radio, labelEdit, valueSpin });

            QObject::connect(removeBtn, &QPushButton::clicked, [&, rowWidget, radio]() {
                rows.erase(std::remove_if(rows.begin(), rows.end(),
                           [radio](const Row& r) { return r.radio == radio; }),
                           rows.end());
                radioGroup->removeButton(radio);
                // Keep one option selected
                if (!rows.empty() && !radioGroup->checkedButton())
                    rows.front().radio->setChecked(true);
                rowWidget->deleteLater();
                rowWidget->hide();
            });
        };

        for (int i = 0; i < (int)dd->items.size(); ++i)
            addRow(dd->items[i].label, dd->items[i].value, i == dd->selected);

        auto* addBtn = new QPushButton("+ Add Option");
        layout->addWidget(addBtn);
        QObject::connect(addBtn, &QPushButton::clicked, [&]() {
            addRow(QString("option %1").arg(rows.size() + 1), 0.0, rows.empty());
        });

        auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
        layout->addWidget(buttons);

        QObject::connect(buttons, &QDialogButtonBox::accepted, [&]() {
            dd->propertyName = nameEdit->text();
            dd->items.clear();
            dd->selected = 0;
            for (int i = 0; i < (int)rows.size(); ++i) {
                dd->items.push_back({ rows[i].label->text(), rows[i].value->value() });
                if (rows[i].radio->isChecked()) dd->selected = i;
            }
            update();
            dialog.accept();
        });
        QObject::connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    }

    // ── Property Number / Slider / Toggle ──────────────────────────────────────
    else if (node_->type == NodeType::PropertyNumber ||
             node_->type == NodeType::PropertySlider ||
             node_->type == NodeType::PropertyToggle) {
        // Each kind has a name; Number/Slider have a value; Slider has range+step;
        // Toggle has an on/off state. Widgets are created per kind below.
        QString* nameRef = nullptr;
        if      (node_->type == NodeType::PropertyNumber) nameRef = &static_cast<PropertyNumberNode*>(node_)->propertyName;
        else if (node_->type == NodeType::PropertySlider) nameRef = &static_cast<PropertySliderNode*>(node_)->propertyName;
        else                                              nameRef = &static_cast<PropertyToggleNode*>(node_)->propertyName;

        layout->addWidget(new QLabel("Property Name:"));
        auto* nameEdit = new QLineEdit(*nameRef);
        layout->addWidget(nameEdit);

        auto makeSpin = [&](const QString& label, double v) {
            layout->addWidget(new QLabel(label));
            auto* s = new QDoubleSpinBox;
            s->setRange(-1e9, 1e9);
            s->setDecimals(4);
            s->setValue(v);
            layout->addWidget(s);
            return s;
        };

        QDoubleSpinBox *valueSpin = nullptr, *minSpin = nullptr, *maxSpin = nullptr, *stepSpin = nullptr;
        QCheckBox* onCheck = nullptr;

        if (node_->type == NodeType::PropertyNumber) {
            valueSpin = makeSpin("Value:", static_cast<PropertyNumberNode*>(node_)->value);
        } else if (node_->type == NodeType::PropertySlider) {
            auto* ps = static_cast<PropertySliderNode*>(node_);
            minSpin   = makeSpin("Minimum:", ps->minValue);
            maxSpin   = makeSpin("Maximum:", ps->maxValue);
            stepSpin  = makeSpin("Step:",    ps->step);
            valueSpin = makeSpin("Value:",   ps->value);
        } else {
            onCheck = new QCheckBox("On");
            onCheck->setChecked(static_cast<PropertyToggleNode*>(node_)->value);
            layout->addWidget(onCheck);
        }

        auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
        layout->addWidget(buttons);

        QObject::connect(buttons, &QDialogButtonBox::accepted, [&]() {
            *nameRef = nameEdit->text();
            if (node_->type == NodeType::PropertyNumber) {
                static_cast<PropertyNumberNode*>(node_)->value = valueSpin->value();
            } else if (node_->type == NodeType::PropertySlider) {
                auto* ps = static_cast<PropertySliderNode*>(node_);
                ps->minValue = minSpin->value();
                ps->maxValue = maxSpin->value();
                ps->step     = stepSpin->value();
                // Keep the value inside the range
                ps->value    = std::clamp(valueSpin->value(), ps->minValue, ps->maxValue);
            } else {
                static_cast<PropertyToggleNode*>(node_)->value = onCheck->isChecked();
            }
            update();
            dialog.accept();
        });
        QObject::connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    }

    // ── Property Text ──────────────────────────────────────────────────────────
    else if (node_->type == NodeType::PropertyText) {
        auto* pt = static_cast<PropertyTextNode*>(node_);
        layout->addWidget(new QLabel("Property Name:"));
        auto* textEdit = new QLineEdit(pt->propertyName);
        layout->addWidget(textEdit);

        auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
        layout->addWidget(buttons);
        QObject::connect(buttons, &QDialogButtonBox::accepted, [&]() {
            pt->propertyName = textEdit->text();
            update();
            dialog.accept();
        });
        QObject::connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    }

    // ── Tooltip Number / Tooltip On-Off ────────────────────────────────────────
    else if (node_->type == NodeType::TooltipNumber || node_->type == NodeType::TooltipOnOff) {
        const bool isOnOff = (node_->type == NodeType::TooltipOnOff);
        QString& labelRef = isOnOff ? static_cast<TooltipOnOffNode*>(node_)->tooltipLabel
                                    : static_cast<TooltipNumberNode*>(node_)->tooltipLabel;

        layout->addWidget(new QLabel("Label:"));
        auto* labelEdit = new QLineEdit(labelRef);
        layout->addWidget(labelEdit);

        QLineEdit* onEdit  = nullptr;
        QLineEdit* offEdit = nullptr;
        if (isOnOff) {
            auto* tt = static_cast<TooltipOnOffNode*>(node_);
            layout->addWidget(new QLabel("Text when on:"));
            onEdit = new QLineEdit(tt->onText);
            layout->addWidget(onEdit);
            layout->addWidget(new QLabel("Text when off:"));
            offEdit = new QLineEdit(tt->offText);
            layout->addWidget(offEdit);
        }

        auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
        layout->addWidget(buttons);
        QObject::connect(buttons, &QDialogButtonBox::accepted, [&]() {
            labelRef = labelEdit->text();
            if (isOnOff) {
                auto* tt = static_cast<TooltipOnOffNode*>(node_);
                tt->onText  = onEdit->text();
                tt->offText = offEdit->text();
            }
            update();
            dialog.accept();
        });
        QObject::connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    }

    // ── I/O nodes: connector label ─────────────────────────────────────────────
    // The name Stormworks shows for this input/output on the microcontroller.
    else if (node_->type == NodeType::BoolInput  || node_->type == NodeType::BoolOutput ||
             node_->type == NodeType::NumberInput || node_->type == NodeType::NumberOutput ||
             node_->type == NodeType::CompositeInput || node_->type == NodeType::CompositeOutput) {
        layout->addWidget(new QLabel("Label (shown in Stormworks):"));
        auto* labelEdit = new QLineEdit(node_->label);
        labelEdit->setPlaceholderText(node_->name);
        layout->addWidget(labelEdit);

        auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
        layout->addWidget(buttons);

        QObject::connect(buttons, &QDialogButtonBox::accepted, [&]() {
            node_->label = labelEdit->text().trimmed();
            update();
            dialog.accept();
        });
        QObject::connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    }

    // ── Lua Script ─────────────────────────────────────────────────────────────
    // The script is stored and saved but not executed by the simulator.
    else if (node_->type == NodeType::LuaScript) {
        auto* lua = static_cast<LuaScriptNode*>(node_);
        dialog.setMinimumWidth(640);

        auto* note = new QLabel("The simulator keeps and saves this script but does not run it.");
        note->setStyleSheet("color: #acacac;");
        layout->addWidget(note);

        auto* editor = new QPlainTextEdit;
        editor->setPlainText(lua->script);
        editor->setLineWrapMode(QPlainTextEdit::NoWrap);
        editor->setMinimumHeight(320);
        QFont mono("Consolas");
        mono.setStyleHint(QFont::Monospace);
        editor->setFont(mono);
        editor->setTabStopDistance(4 * QFontMetricsF(mono).horizontalAdvance(' '));
        layout->addWidget(editor);

        auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
        layout->addWidget(buttons);
        QObject::connect(buttons, &QDialogButtonBox::accepted, [&]() {
            lua->script = editor->toPlainText();
            update();
            dialog.accept();
        });
        QObject::connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    }

    // ── Blinker ────────────────────────────────────────────────────────────────
    else if (node_->type == NodeType::Blinker) {
        auto* bl = static_cast<BlinkerNode*>(node_);

        layout->addWidget(new QLabel("On Time (seconds):"));
        auto* onSpin = new QDoubleSpinBox;
        onSpin->setRange(BlinkerNode::kMinTime, BlinkerNode::kMaxTime);
        onSpin->setDecimals(3);
        onSpin->setSingleStep(0.1);
        onSpin->setValue(bl->onTime);
        layout->addWidget(onSpin);

        layout->addWidget(new QLabel("Off Time (seconds):"));
        auto* offSpin = new QDoubleSpinBox;
        offSpin->setRange(BlinkerNode::kMinTime, BlinkerNode::kMaxTime);
        offSpin->setDecimals(3);
        offSpin->setSingleStep(0.1);
        offSpin->setValue(bl->offTime);
        layout->addWidget(offSpin);

        auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
        layout->addWidget(buttons);
        QObject::connect(buttons, &QDialogButtonBox::accepted, [&]() {
            bl->onTime  = onSpin->value();
            bl->offTime = offSpin->value();
            update();
            dialog.accept();
        });
        QObject::connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    }

    // ── Threshold ──────────────────────────────────────────────────────────────
    else if (node_->type == NodeType::Threshold) {
        auto* th = static_cast<ThresholdNode*>(node_);

        layout->addWidget(new QLabel("Lower Bound:"));
        auto* minSpin = new QDoubleSpinBox;
        minSpin->setRange(-1e9, 1e9);
        minSpin->setDecimals(4);
        minSpin->setValue(th->minBound);
        layout->addWidget(minSpin);

        layout->addWidget(new QLabel("Upper Bound:"));
        auto* maxSpin = new QDoubleSpinBox;
        maxSpin->setRange(-1e9, 1e9);
        maxSpin->setDecimals(4);
        maxSpin->setValue(th->maxBound);
        layout->addWidget(maxSpin);

        auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
        layout->addWidget(buttons);

        QObject::connect(buttons, &QDialogButtonBox::accepted, [&]() {
            th->minBound = minSpin->value();
            th->maxBound = maxSpin->value();
            update();
            dialog.accept();
        });
        QObject::connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    }

    // ── Capacitor ──────────────────────────────────────────────────────────────
    else if (node_->type == NodeType::Capacitor) {
        auto* cap = static_cast<CapacitorNode*>(node_);

        layout->addWidget(new QLabel("Charge Time (seconds):"));
        auto* chargeSpin = new QDoubleSpinBox;
        chargeSpin->setRange(0.0, 1e6);
        chargeSpin->setDecimals(3);
        chargeSpin->setSingleStep(0.1);
        chargeSpin->setValue(cap->chargeTime);
        layout->addWidget(chargeSpin);

        layout->addWidget(new QLabel("Discharge Time (seconds):"));
        auto* dischargeSpin = new QDoubleSpinBox;
        dischargeSpin->setRange(0.0, 1e6);
        dischargeSpin->setDecimals(3);
        dischargeSpin->setSingleStep(0.1);
        dischargeSpin->setValue(cap->dischargeTime);
        layout->addWidget(dischargeSpin);

        auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
        layout->addWidget(buttons);

        QObject::connect(buttons, &QDialogButtonBox::accepted, [&]() {
            cap->chargeTime    = chargeSpin->value();
            cap->dischargeTime = dischargeSpin->value();
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

    // ── Function Evaluator Nodes ───────────────────────────────────────────────
    else if (node_->type == NodeType::Function1 || node_->type == NodeType::Function3 ||
             node_->type == NodeType::Function8  || node_->type == NodeType::FunctionBool4 ||
             node_->type == NodeType::FunctionBool8) {

        dialog.setMinimumWidth(400);

        // Force the dialog to always fit its content — this is what allows
        // it to shrink back down when collapsible sections are closed
        layout->setSizeConstraint(QLayout::SetFixedSize);

        FunctionNodeBase* fn = static_cast<FunctionNodeBase*>(node_);

        // Show which variables are available based on node type
        QString varHint;
        switch (node_->type) {
            case NodeType::Function1:     varHint = "Variable: x"; break;
            case NodeType::Function3:     varHint = "Variables: x, y, z"; break;
            case NodeType::Function8:     varHint = "Variables: x, y, z, w, a, b, c, d"; break;
            case NodeType::FunctionBool4: varHint = "Variables: x, y, z, w (bool → 0/1)"; break;
            case NodeType::FunctionBool8: varHint = "Variables: x,y,z,w,a,b,c,d (bool → 0/1)"; break;
            default: break;
        }

        auto* hintLbl = new QLabel(varHint);
        hintLbl->setStyleSheet("color: #acacac; font-size: 14px;");
        layout->addWidget(hintLbl);

        layout->addWidget(new QLabel("Expression:"));
        auto* exprEdit = new QLineEdit;
        exprEdit->setText(fn->expression);
        exprEdit->setPlaceholderText("e.g. (x * 4) - 5");
        exprEdit->setMinimumWidth(400);
        layout->addWidget(exprEdit);

        // ── Collapsible reference sections ────────────────────────────────────
        // Helper that creates a toggle button + hidden body widget pair
        auto makeSection = [&](const QString& title, const QString& content) {
            auto* sectionWidget = new QWidget;
            auto* sectionLayout = new QVBoxLayout(sectionWidget);
            sectionLayout->setContentsMargins(0, 0, 0, 0);
            sectionLayout->setSpacing(0);

            auto* toggle = new QPushButton("▶  " + title);
            toggle->setCheckable(true);
            toggle->setChecked(false);
            toggle->setStyleSheet(
                "QPushButton { background: #2e2e2e; color: #aaa; border: none; "
                "  text-align: left; padding: 4px 6px; font-size: 14px; }"
                "QPushButton:checked { color: #ddd; background: #383838; }"
                "QPushButton:hover { background: #383838; }");
            sectionLayout->addWidget(toggle);

            auto* body = new QLabel(content);
            body->setStyleSheet("color: #cfcfcf; font-size: 14px; padding: 4px 10px;");
            body->setWordWrap(false);
            body->setVisible(false);
            sectionLayout->addWidget(body);

            QObject::connect(toggle, &QPushButton::toggled, [toggle, body, &dialog](bool on) {
                body->setVisible(on);
                toggle->setText((on ? "▼  " : "▶  ") + toggle->text().mid(3));
                
                // Allow the layout to recalculate
                QCoreApplication::processEvents();
            
                if (!on) {
                    // FORCE the dialog to shrink back down to its new minimum vertical size
                    // while keeping its current width
                    dialog.resize(dialog.width(), dialog.minimumSizeHint().height());
                } else {
                    dialog.adjustSize();
                }
            });

            layout->addWidget(sectionWidget);
        };

        makeSection("Operators",
            "+   Addition\n"
            "-   Subtraction\n"
            "*   Multiply\n"
            "/   Divide\n"
            "%   Modulo\n"
            "^   Power");

        makeSection("Constants",
            "pi      3.14159...\n"
            "pi2     Pi*2\n");

        makeSection("Trigonometry",
            "sin(x)      Sine\n"
            "cos(x)      Cosine\n"
            "tan(x)      Tangent\n"
            "asin(x)     Arcsine\n"
            "acos(x)     Arccosine\n"
            "atan(x)     Arctangent\n"
            "atan2(x,y)  Two-argument arctangent");

        makeSection("Math Functions",
            "abs(x)        absolute value\n"
            "sgn(x)        sign of x (-1, 0, or 1)\n"
            "sqrt(x)       square root\n"
            "ceil(x)       ceiling\n"
            "floor(x)      floor\n"
            "round(x)      round to nearest integer\n"
            "max(x,y)      larger of x and y\n"
            "min(x,y)      smaller of x and y");

        makeSection("Vector & Interpolation",
            "len(x,y)      length of vector (x,y)\n"
            "len2(x,y)     squared length of vector (x,y)\n"
            "lerp(x,y,z)   linear interpolation from x to y by factor z\n"
            "clamp(x,y,z)  clamp x between y and z");

        makeSection("Conversions",
            "X*3.6         m/s to kmph\n"
            "X*2.236936    m/s to mph\n"
            "X*1.943844    m/s to knots\n"
            "Pi2           Turns to Radians\n"
            "X*(Pi/180)    Degrees to Radians\n"
            "X*(180/Pi)    Radians to Degrees\n");

        auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
        layout->addWidget(buttons);

        QObject::connect(buttons, &QDialogButtonBox::accepted, [&]() {
            fn->expression = exprEdit->text().trimmed();
            update();
            dialog.accept();
        });
        QObject::connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    }

    dialog.exec();
}
