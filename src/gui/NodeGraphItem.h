#pragma once

#include <QGraphicsItem>
#include <QPointF>
#include <QString>
#include "../core/Node.h"

// ─────────────────────────────────────────────────────────────────────────────
// NodeGraphItem
//
// Draws a single simulation node as a rounded rectangle with:
//   - Colour-coded header bar by node category
//   - Input ports on the left, output ports on the right
//   - A small ⚙ settings button in the header for configurable nodes
//
// Configurable nodes (NumberLiteral, CompositeRead*, CompositePacker):
//   Clicking the ⚙ button opens a QDialog with relevant settings.
// ─────────────────────────────────────────────────────────────────────────────
class NodeGraphItem : public QGraphicsItem {
public:
    // ── Grid-based sizing ──────────────────────────────────────────────────────
    // Grid cell size is 40x40. Node width is fixed at 4 cells (160px).
    // Height starts at 2 cells (80px) for a single port row, then grows by
    // 1 cell (40px) for every 2 additional port rows — see totalHeight().
    static constexpr int kGridCell   = 40;
    // Stormworks nodes are about 4 grid squares wide
    static constexpr int kWidth      = kGridCell * 4;  // 160
    static constexpr int kHeaderH    = 28;
    // Used only as a label-sizing reference (text box height within a row);
    // actual port row spacing is computed dynamically in inputPortY/outputPortY
    // since rows are spaced evenly across whatever total height totalHeight()
    // produces, not a fixed per-row height.
    static constexpr int kPortRowH   = kGridCell;
    static constexpr int kPortRadius = 5;
    static constexpr int kPadding    = 8;
    static constexpr int kGearSize   = 18; // settings button size

    // Grey display panel in the middle of the node (see innerRect())
    // The grey panel keeps a fixed width and sits centred; whatever is left on
    // each side holds the port rings and their (short) labels.
    static constexpr qreal kInnerPanelW  = 86.4;
    static constexpr qreal kInnerMarginX = (kWidth - kInnerPanelW) / 2.0;
    // The panel shrinks to make room for long port labels, but not below this
    static constexpr qreal kMinPanelW    = 50.0;
    static constexpr qreal kInnerMarginY = 4.0;
    static constexpr qreal kInnerCircleR = 4.0;
    static constexpr qreal kInnerPad     = 5.0;
    static constexpr qreal kInnerRingR   = kInnerCircleR - 1; // ring radius (centre line)
    static constexpr qreal kInnerRingW   = 2.5;               // ring thickness
    static constexpr qreal kGateLineW    = 2.5;               // logic gate symbol line width

    explicit NodeGraphItem(Node* node, QGraphicsItem* parent = nullptr);

    QRectF boundingRect() const override;
    void   paint(QPainter* painter,
                 const QStyleOptionGraphicsItem* option,
                 QWidget* widget = nullptr) override;

    QPointF inputPortScenePos (int portIndex) const;
    QPointF outputPortScenePos(int portIndex) const;

    // ── Port hit-testing (used for wire dragging) ─────────────────────────────
    // Returns the input/output port index under the given scene position,
    // or -1 if no port is close enough. usedForHitTest is a slightly larger
    // radius than the visual port so clicking is forgiving.
    int inputPortAt (const QPointF& scenePos) const;
    int outputPortAt(const QPointF& scenePos) const;

    // Returns the wire color for a given input or output port (red/green/purple)
    // matching the logic in paint() — used to enforce same-color connections.
    QColor portColorAt(int portIndex, bool isOutput) const;

    // Whether this node hides its input or output ports entirely
    // (pure source nodes have no visible input; pure sink nodes have no visible output)
    bool inputPortsVisible()  const;
    bool outputPortsVisible() const;

    Node* node()   const { return node_; }
    int   nodeId() const { return node_->id; }

    void refresh() { update(); }

protected:
    QVariant itemChange(GraphicsItemChange change, const QVariant& value) override;

    // Handle click on the settings gear button
    void mousePressEvent(QGraphicsSceneMouseEvent* event) override;

    // Port labels are shortened to fit the narrow node; hovering a port shows
    // its full label as a tooltip
    void hoverMoveEvent(QGraphicsSceneHoverEvent* event) override;

private:
    Node* node_;

    int    totalHeight() const;
    QRectF  innerRect() const;
    void    panelMargins(qreal& left, qreal& right) const; // space beside the panel
    QPointF inputPortLocalPos(int idx) const; // circle inside the grey panel
    qreal  inputPortY (int idx) const;
    qreal  outputPortY(int idx) const;
    QColor headerColor() const;
    QRectF gearButtonRect() const;

    // Returns true if this node type has configurable settings
    bool hasSettings() const;

    // Opens the settings dialog for this node
    void openSettingsDialog();
};
