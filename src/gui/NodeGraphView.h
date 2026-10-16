#pragma once

#include <QGraphicsView>
#include <QGraphicsScene>
#include <QGraphicsPathItem>
#include <QMenu>
#include <QPointF>
#include <functional>
#include <unordered_map>

#include "../core/Microcontroller.h"
class NodePalette;
#include "NodeGraphItem.h"
#include "ConnectionItem.h"

// ─────────────────────────────────────────────────────────────────────────────
// NodeGraphView
//
// A QGraphicsView that renders the full node graph of a Microcontroller.
//
// Responsibilities:
//   • Build NodeGraphItems and ConnectionItems from the Microcontroller model
//   • Handle middle-mouse pan and scroll-wheel zoom
//   • Handle wire creation/deletion/rewiring via left-click drag on ports
//   • Draw a background grid and snap all node positions to it
//   • Right-click context menu to add new nodes (grouped by category) or
//     delete the node under the cursor
//   • Call refresh() on all NodeGraphItems after each simulation tick so
//     port values update in real time
//
// Wire interaction model:
//   • Click an OUTPUT port (no drag in progress) → start a new wire from it.
//   • Click an INPUT port that already has a wire → detach that wire; the
//     output end stays fixed, the input end now follows the pointer.
//   • While dragging, hovering a compatible (same-color) input port shows
//     a connection preview; releasing there completes the connection,
//     replacing any existing wire on that input.
//   • Releasing on an incompatible port is ignored — the drag continues.
//   • Releasing on empty space deletes the wire being dragged.
//
// Grid:
//   • Cell size is fixed at kGridCellW x kGridCellH, sized so the smallest
//     node (a single-port I/O node) occupies a 2-wide x 1-tall cell block.
//   • All node positions — on load, on add, and after any drag — are
//     snapped to the nearest grid intersection.
// ─────────────────────────────────────────────────────────────────────────────
class NodeGraphView : public QGraphicsView {
    Q_OBJECT

public:
    explicit NodeGraphView(QWidget* parent = nullptr);

    void loadMicrocontroller(Microcontroller* mc);
    void refreshNodes();
    void clearGraph();

    // Fills a menu with the categorized list of node types. Choosing one adds
    // that node at the scene position `spawnPos()` returns when it is clicked.
    // Used by both the right-click menu and the menu bar's "Add Node" menu.
    void populateAddNodeMenu(QMenu* menu, std::function<QPointF()> spawnPos);

    // A good place to put a node added from the menu bar: the middle of the
    // visible area, grid-snapped, nudged to a free spot if it's occupied.
    QPointF defaultSpawnPoint() const;

    // Opens the Tab palette (or closes it if open)
    void togglePalette();

    // ── Grid sizing ────────────────────────────────────────────────────────────
    // Fixed 40x40 square cells. Node footprints are now multiples of this:
    // a single-port I/O node is 4 cells wide x 2 cells tall, growing taller
    // by 1 cell for every 2 additional port rows (see NodeGraphItem).
    static constexpr int kGridCellW = NodeGraphItem::kGridCell;
    static constexpr int kGridCellH = NodeGraphItem::kGridCell;

    // ── Board size ─────────────────────────────────────────────────────────────
    // Like the Stormworks editor, the work area is a fixed-size board: the grid
    // stops at its edge, nodes can't be dragged off it, and zoom/pan are limited
    // to it. Sizes are in grid cells; the board is placed so the area where
    // loaded files put their nodes sits near the middle.
    static constexpr int kBoardLeftCells = -32;
    static constexpr int kBoardTopCells  = -20;
    static constexpr int kBoardWCells    = 80;
    static constexpr int kBoardHCells    = 48;
    static QRectF boardRect() {
        return QRectF(kBoardLeftCells * kGridCellW, kBoardTopCells * kGridCellH,
                      kBoardWCells * kGridCellW,    kBoardHCells * kGridCellH);
    }

signals:
    // Emitted whenever a node is added or removed, so MainWindow can tell
    // IOPanel to rebuild its input/output controls.
    void nodeSetChanged();

protected:
    void resizeEvent      (QResizeEvent* event) override;
    void enterEvent       (QEnterEvent*  event) override;
    void wheelEvent       (QWheelEvent*  event) override;
    void mousePressEvent  (QMouseEvent*  event) override;
    void mouseMoveEvent   (QMouseEvent*  event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void keyPressEvent    (QKeyEvent*    event) override;

    // Draws the background grid beneath all items
    void drawBackground(QPainter* painter, const QRectF& rect) override;

private:
    QGraphicsScene*  scene_;
    Microcontroller* mc_ = nullptr; // non-owning, set by loadMicrocontroller

    // Tab palette of node tiles, shown over the view; nodes it adds go at
    // paletteSpawn_ (where the mouse was when it opened)
    NodePalette* palette_ = nullptr;
    QPointF      paletteSpawn_;

    void    positionPalette();
    QPointF freeSpotNear(QPointF spot) const;

    // Maps node id → its visual item so we can look up endpoints for wires
    std::unordered_map<int, NodeGraphItem*> nodeItems_;

    // All live connection wires currently drawn — kept separately from the
    // scene's item list so we can find/remove a specific wire by endpoint.
    std::vector<ConnectionItem*> connectionItems_;

    // Pan state
    bool   panning_ = false;
    QPoint lastMousePos_;

    // ── Wire-dragging state ───────────────────────────────────────────────────
    bool    dragging_       = false;
    QColor  dragColor_;             // color the dragged wire must match to connect
    QPointF dragFixedEndScene_;     // the end of the wire that stays put (scene coords)
    bool    dragFixedIsOutput_;     // true if the fixed end is an output port
    int     dragFixedNodeId_  = -1;
    int     dragFixedPortIdx_ = -1;
    QGraphicsPathItem* dragPreview_ = nullptr; // temporary wire shown while dragging

    // Currently selected node (for Delete key support), or -1 if none
    int selectedNodeId_ = -1;

    // ── Helpers ───────────────────────────────────────────────────────────────

    // Find which node+port (if any) is under the given scene position.
    bool findPortAt(const QPointF& scenePos,
                    NodeGraphItem*& outItem, int& outPortIdx, bool& outIsOutput) const;

    // Find which node (if any) is under the given scene position, ignoring ports
    NodeGraphItem* findNodeAt(const QPointF& scenePos) const;

    // Remove and delete a connection matching the given endpoint criteria.
    void removeConnectionToInput(int targetNodeId, int targetPortIdx);

    // Create the actual Connection in the model and the ConnectionItem visual
    void createConnection(int sourceNodeId, int sourcePortIdx,
                          int targetNodeId, int targetPortIdx);

    // Update the temporary preview path while dragging
    void updateDragPreview(const QPointF& currentScenePos);

    // Clean up drag state (called on release or cancel)
    void endDrag();

    // Snap a scene position to the nearest grid intersection
    QPointF snapToGrid(const QPointF& scenePos) const;

    // Show the right-click context menu at the given view-space position.
    // sceneClickPos is where the menu was invoked, used as the spawn point
    // for newly added nodes and to find a node-to-delete if right-clicked
    // directly on one.
    void showContextMenu(const QPoint& viewPos, const QPointF& sceneClickPos);

    // Add a new node of the given type at the (already snapped) scene position
    void addNodeAt(NodeType type, const QPointF& snappedScenePos);

    // Delete a node entirely: removes all connections touching it, removes
    // its NodeGraphItem and any ConnectionItems, and removes it from the model.
    void deleteNode(int nodeId);
};
