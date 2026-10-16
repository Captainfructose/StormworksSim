#include "NodeGraphView.h"

#include <QWheelEvent>
#include <QMouseEvent>
#include <QKeyEvent>
#include <QScrollBar>
#include <QPainterPath>
#include <QPen>
#include <QMenu>
#include <QAction>

// ─────────────────────────────────────────────────────────────────────────────
// Constructor
// ─────────────────────────────────────────────────────────────────────────────
NodeGraphView::NodeGraphView(QWidget* parent)
    : QGraphicsView(parent)
{
    scene_ = new QGraphicsScene(this);
    setScene(scene_);

    setBackgroundBrush(QColor(0x62, 0xb6, 0xc9)); // Stormworks teal
    setRenderHint(QPainter::Antialiasing);
    setDragMode(QGraphicsView::NoDrag);
    setTransformationAnchor(QGraphicsView::AnchorUnderMouse);
    setResizeAnchor(QGraphicsView::AnchorUnderMouse);
    // The scene is exactly the board; items and the view are confined to it
    scene_->setSceneRect(boardRect());
    setMouseTracking(true);

    // Allow this widget to receive key events (Delete key for node removal)
    setFocusPolicy(Qt::StrongFocus);
}

// ─────────────────────────────────────────────────────────────────────────────
// drawBackground — the board: teal grid inside, a dark margin outside it
// ─────────────────────────────────────────────────────────────────────────────
void NodeGraphView::drawBackground(QPainter* painter, const QRectF& rect)
{
    const QRectF board = boardRect();

    // Everything off the board is a flat dark colour
    QPainterPath outside;
    outside.addRect(rect);
    QPainterPath boardPath;
    boardPath.addRect(board);
    painter->fillPath(outside.subtracted(boardPath), QColor(0x1f, 0x2a, 0x30));

    // The board itself, and its grid lines (kept inside the board)
    const QRectF area = rect.intersected(board);
    if (area.isEmpty()) return;
    painter->fillRect(area, backgroundBrush());

    QPen gridPen(QColor(0x46, 0x8c, 0xa0)); // darker teal, as in Stormworks
    gridPen.setWidth(0); // cosmetic, always 1px regardless of zoom
    painter->setPen(gridPen);

    // Align the first line to the nearest grid boundary at or before the edge
    qreal left = std::floor(area.left() / kGridCellW) * kGridCellW;
    qreal top  = std::floor(area.top()  / kGridCellH) * kGridCellH;

    for (qreal x = left; x <= area.right(); x += kGridCellW)
        painter->drawLine(QPointF(x, area.top()), QPointF(x, area.bottom()));

    for (qreal y = top; y <= area.bottom(); y += kGridCellH)
        painter->drawLine(QPointF(area.left(), y), QPointF(area.right(), y));

    // A darker outline marks the edge of the board
    QPen edgePen(QColor(0x2e, 0x66, 0x78));
    edgePen.setWidth(0);
    painter->setPen(edgePen);
    painter->setBrush(Qt::NoBrush);
    painter->drawRect(board);
}

// ─────────────────────────────────────────────────────────────────────────────
// snapToGrid — round a scene position to the nearest grid intersection, kept on
// the board so a node's top-left never lands outside it
// ─────────────────────────────────────────────────────────────────────────────
QPointF NodeGraphView::snapToGrid(const QPointF& scenePos) const
{
    const QRectF board = boardRect();
    qreal x = std::round(scenePos.x() / kGridCellW) * kGridCellW;
    qreal y = std::round(scenePos.y() / kGridCellH) * kGridCellH;
    x = std::clamp(x, board.left(), board.right()  - kGridCellW);
    y = std::clamp(y, board.top(),  board.bottom() - kGridCellH);
    return QPointF(x, y);
}

// ─────────────────────────────────────────────────────────────────────────────
// loadMicrocontroller
// ─────────────────────────────────────────────────────────────────────────────
void NodeGraphView::loadMicrocontroller(Microcontroller* mc)
{
    clearGraph();
    mc_ = mc;
    if (!mc) return;

    for (const auto& [id, nodePtr] : mc->nodes()) {
        // Snap every loaded node's position to the grid immediately
        QPointF snapped = snapToGrid(QPointF(nodePtr->posX, nodePtr->posY));
        nodePtr->posX = (int)snapped.x();
        nodePtr->posY = (int)snapped.y();

        auto* item = new NodeGraphItem(nodePtr.get());
        scene_->addItem(item);
        nodeItems_[id] = item;
    }

    for (const auto& conn : mc->connections()) {
        auto srcIt = nodeItems_.find(conn.sourceNodeId);
        auto dstIt = nodeItems_.find(conn.targetNodeId);
        if (srcIt == nodeItems_.end() || dstIt == nodeItems_.end()) continue;

        auto* wire = new ConnectionItem(srcIt->second, conn.sourcePortIdx,
                                        dstIt->second, conn.targetPortIdx);
        scene_->addItem(wire);
        connectionItems_.push_back(wire);
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// refreshNodes
// ─────────────────────────────────────────────────────────────────────────────
void NodeGraphView::refreshNodes()
{
    for (auto& [id, item] : nodeItems_) {
        item->refresh();
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// clearGraph
// ─────────────────────────────────────────────────────────────────────────────
void NodeGraphView::clearGraph()
{
    scene_->clear();
    nodeItems_.clear();
    connectionItems_.clear();
    dragPreview_ = nullptr;
    dragging_ = false;
    selectedNodeId_ = -1;
    mc_ = nullptr;
}

// ─────────────────────────────────────────────────────────────────────────────
// findPortAt
// ─────────────────────────────────────────────────────────────────────────────
bool NodeGraphView::findPortAt(const QPointF& scenePos,
                               NodeGraphItem*& outItem, int& outPortIdx, bool& outIsOutput) const
{
    for (auto& [id, item] : nodeItems_) {
        if (!item) continue;
        int outIdx = item->outputPortAt(scenePos);
        if (outIdx >= 0) {
            outItem = item;
            outPortIdx = outIdx;
            outIsOutput = true;
            return true;
        }
        int inIdx = item->inputPortAt(scenePos);
        if (inIdx >= 0) {
            outItem = item;
            outPortIdx = inIdx;
            outIsOutput = false;
            return true;
        }
    }
    return false;
}

// ─────────────────────────────────────────────────────────────────────────────
// findNodeAt — locate the NodeGraphItem (if any) whose bounding box contains
// scenePos, regardless of whether the click landed on a port specifically.
// Used for right-click "delete this node" and Delete-key support.
// ─────────────────────────────────────────────────────────────────────────────
NodeGraphItem* NodeGraphView::findNodeAt(const QPointF& scenePos) const
{
    QGraphicsItem* hit = scene_->itemAt(scenePos, QTransform());
    return qgraphicsitem_cast<NodeGraphItem*>(hit);
}

// ─────────────────────────────────────────────────────────────────────────────
// removeConnectionToInput
// ─────────────────────────────────────────────────────────────────────────────
void NodeGraphView::removeConnectionToInput(int targetNodeId, int targetPortIdx)
{
    if (!mc_) return;

    for (size_t i = 0; i < connectionItems_.size(); ++i) {
        ConnectionItem* item = connectionItems_[i];
        if (item->targetNodeId() == targetNodeId && item->targetPortIdx() == targetPortIdx) {
            scene_->removeItem(item);
            delete item;
            connectionItems_.erase(connectionItems_.begin() + i);
            break;
        }
    }

    mc_->removeConnectionToInput(targetNodeId, targetPortIdx);
}

// ─────────────────────────────────────────────────────────────────────────────
// createConnection
// ─────────────────────────────────────────────────────────────────────────────
void NodeGraphView::createConnection(int sourceNodeId, int sourcePortIdx,
                                     int targetNodeId, int targetPortIdx)
{
    if (!mc_) return;

    Connection conn;
    conn.sourceNodeId  = sourceNodeId;
    conn.sourcePortIdx = sourcePortIdx;
    conn.targetNodeId  = targetNodeId;
    conn.targetPortIdx = targetPortIdx;
    mc_->addConnection(conn);

    auto srcIt = nodeItems_.find(sourceNodeId);
    auto dstIt = nodeItems_.find(targetNodeId);
    if (srcIt == nodeItems_.end() || dstIt == nodeItems_.end()) return;

    auto* wire = new ConnectionItem(srcIt->second, sourcePortIdx,
                                    dstIt->second, targetPortIdx);
    scene_->addItem(wire);
    connectionItems_.push_back(wire);
}

// ─────────────────────────────────────────────────────────────────────────────
// addNodeAt — create a new node of the given type at a grid-snapped position
// ─────────────────────────────────────────────────────────────────────────────
void NodeGraphView::addNodeAt(NodeType type, const QPointF& snappedScenePos)
{
    if (!mc_) return;

    auto node = Node::create(type);
    if (!node) return;

    node->posX = (int)snappedScenePos.x();
    node->posY = (int)snappedScenePos.y();

    int newId = mc_->addNode(std::move(node));
    Node* rawNode = mc_->node(newId);
    if (!rawNode) return;

    auto* item = new NodeGraphItem(rawNode);
    scene_->addItem(item);
    nodeItems_[newId] = item;

    emit nodeSetChanged();
}

// ─────────────────────────────────────────────────────────────────────────────
// deleteNode — remove a node, its visual item, and every wire touching it
// ─────────────────────────────────────────────────────────────────────────────
void NodeGraphView::deleteNode(int nodeId)
{
    if (!mc_) return;

    // Remove every ConnectionItem that touches this node, in either direction
    for (size_t i = 0; i < connectionItems_.size(); ) {
        ConnectionItem* item = connectionItems_[i];
        if (item->sourceNodeId() == nodeId || item->targetNodeId() == nodeId) {
            scene_->removeItem(item);
            delete item;
            connectionItems_.erase(connectionItems_.begin() + i);
        } else {
            ++i;
        }
    }

    // Remove the visual node item
    auto it = nodeItems_.find(nodeId);
    if (it != nodeItems_.end()) {
        scene_->removeItem(it->second);
        delete it->second;
        nodeItems_.erase(it);
    }

    // Remove from the model (also cleans up any connections in the model
    // that our visual cleanup above might have missed, e.g. if a load
    // produced a connection to a now-missing node item)
    mc_->removeNode(nodeId);

    if (selectedNodeId_ == nodeId) selectedNodeId_ = -1;

    emit nodeSetChanged();
}

// ─────────────────────────────────────────────────────────────────────────────
// updateDragPreview
// ─────────────────────────────────────────────────────────────────────────────
void NodeGraphView::updateDragPreview(const QPointF& currentScenePos)
{
    if (!dragPreview_) return;

    QPainterPath path;
    QPointF p1 = dragFixedEndScene_;
    QPointF p2 = currentScenePos;

    qreal dx = std::abs(p2.x() - p1.x()) * 0.5;
    dx = std::max(dx, 40.0);

    QPointF ctrl1 = dragFixedIsOutput_ ? QPointF(p1.x() + dx, p1.y())
                                       : QPointF(p1.x() - dx, p1.y());
    QPointF ctrl2 = dragFixedIsOutput_ ? QPointF(p2.x() - dx, p2.y())
                                       : QPointF(p2.x() + dx, p2.y());

    path.moveTo(p1);
    path.cubicTo(ctrl1, ctrl2, p2);
    dragPreview_->setPath(path);
}

// ─────────────────────────────────────────────────────────────────────────────
// endDrag
// ─────────────────────────────────────────────────────────────────────────────
void NodeGraphView::endDrag()
{
    if (dragPreview_) {
        scene_->removeItem(dragPreview_);
        delete dragPreview_;
        dragPreview_ = nullptr;
    }
    dragging_ = false;
    dragFixedNodeId_  = -1;
    dragFixedPortIdx_ = -1;
}

// ─────────────────────────────────────────────────────────────────────────────
// showContextMenu
//
// Right-click menu. If the click landed on an existing node, offers "Delete
// Node". Always offers "Add Node" with a submenu grouped by category;
// selecting an entry spawns that node type at the clicked (grid-snapped)
// position.
// ─────────────────────────────────────────────────────────────────────────────
void NodeGraphView::showContextMenu(const QPoint& viewPos, const QPointF& sceneClickPos)
{
    if (!mc_) return;

    QMenu menu(this);
    menu.setStyleSheet(
        "QMenu { background-color: #252525; color: #cccccc; border: 1px solid #444; }"
        "QMenu::item { padding: 4px 24px 4px 12px; }"
        "QMenu::item:selected { background: #3a3a3a; }");

    NodeGraphItem* hitNode = findNodeAt(sceneClickPos);
    if (hitNode) {
        QAction* deleteAct = menu.addAction(QString("Delete \"%1\"").arg(hitNode->node()->name));
        connect(deleteAct, &QAction::triggered, this, [this, hitNode]() {
            deleteNode(hitNode->nodeId());
        });
        menu.addSeparator();
    }

    QPointF snappedPos = snapToGrid(sceneClickPos);

    // ── Add Node submenu, grouped by category ─────────────────────────────────
    QMenu* addMenu = menu.addMenu("Add Node");

    auto addEntry = [&](QMenu* parentMenu, const QString& label, NodeType type) {
        QAction* act = parentMenu->addAction(label);
        connect(act, &QAction::triggered, this, [this, type, snappedPos]() {
            addNodeAt(type, snappedPos);
        });
    };

    QMenu* ioMenu = addMenu->addMenu("I/O");
    addEntry(ioMenu, "Bool Input",      NodeType::BoolInput);
    addEntry(ioMenu, "Bool Output",     NodeType::BoolOutput);
    addEntry(ioMenu, "Number Input",    NodeType::NumberInput);
    addEntry(ioMenu, "Number Output",   NodeType::NumberOutput);
    addEntry(ioMenu, "Composite Input",  NodeType::CompositeInput);
    addEntry(ioMenu, "Composite Output", NodeType::CompositeOutput);

    QMenu* constantsMenu = addMenu->addMenu("Constants");
    addEntry(constantsMenu, "Constant Number", NodeType::NumberLiteral);
    addEntry(constantsMenu, "Constant On",     NodeType::ConstantOn);

    QMenu* compositeMenu = addMenu->addMenu("Composite");
    addEntry(compositeMenu, "Composite Packer",      NodeType::CompositePacker);
    addEntry(compositeMenu, "Composite Read Number", NodeType::CompositeReadNumber);
    addEntry(compositeMenu, "Composite Read Bool",   NodeType::CompositeReadBool);
    addEntry(compositeMenu, "Composite Write Number", NodeType::CompositeWriteNumber);
    addEntry(compositeMenu, "Composite Write Bool",   NodeType::CompositeWriteBool);
    addEntry(compositeMenu, "Composite Switchbox",    NodeType::CompositeSwitchbox);
    addEntry(compositeMenu, "Lua Script",             NodeType::LuaScript);
    addEntry(compositeMenu, "Binary to Number",       NodeType::BinaryToNumber);
    addEntry(compositeMenu, "Number to Binary",       NodeType::NumberToBinary);

    QMenu* logicMenu = addMenu->addMenu("Logic");
    addEntry(logicMenu, "AND",  NodeType::And);
    addEntry(logicMenu, "OR",   NodeType::Or);
    addEntry(logicMenu, "NOT",  NodeType::Not);
    addEntry(logicMenu, "XOR",  NodeType::Xor);
    addEntry(logicMenu, "NAND", NodeType::Nand);
    addEntry(logicMenu, "NOR",  NodeType::Nor);
    addEntry(logicMenu, "SR Latch",     NodeType::SRLatch);
    addEntry(logicMenu, "JK Flip-Flop", NodeType::JKFlipFlop);
    addEntry(logicMenu, "Capacitor",    NodeType::Capacitor);
    addEntry(logicMenu, "Push to Toggle", NodeType::PushToToggle);
    addEntry(logicMenu, "Toggle to Push", NodeType::ToggleToPush);

    QMenu* mathMenu = addMenu->addMenu("Math");
    addEntry(mathMenu, "Add",          NodeType::Add);
    addEntry(mathMenu, "Subtract",     NodeType::Subtract);
    addEntry(mathMenu, "Multiply",     NodeType::Multiply);
    addEntry(mathMenu, "Divide",       NodeType::Divide);
    addEntry(mathMenu, "Clamp",        NodeType::Clamp);
    addEntry(mathMenu, "Abs",          NodeType::Abs);
    addEntry(mathMenu, "Modulo",       NodeType::Modulo);
    addEntry(mathMenu, "Delta",        NodeType::Delta);
    addEntry(mathMenu, "Threshold",    NodeType::Threshold);
    addEntry(mathMenu, "Switchbox",    NodeType::NumericalSwitchbox);
    addEntry(mathMenu, "Greater Than", NodeType::GreaterThan);
    addEntry(mathMenu, "Less Than",    NodeType::LessThan);
    addEntry(mathMenu, "Equal To",     NodeType::EqualTo);

    QMenu* statefulMenu = addMenu->addMenu("Stateful");
    addEntry(statefulMenu, "Up/Down Counter",     NodeType::UpDownCounter);
    addEntry(statefulMenu, "Memory Register",     NodeType::MemoryRegister);
    addEntry(statefulMenu, "Blinker",             NodeType::Blinker);
    statefulMenu->addSeparator();
    addEntry(statefulMenu, "TON Timer",           NodeType::TimerTON);
    addEntry(statefulMenu, "TOF Timer",           NodeType::TimerTOF);
    addEntry(statefulMenu, "RTO Timer",           NodeType::TimerRTO);
    addEntry(statefulMenu, "RTF Timer",           NodeType::TimerRTF);
    statefulMenu->addSeparator();
    addEntry(statefulMenu, "PID Controller",      NodeType::PIDController);
    addEntry(statefulMenu, "PID Advanced",        NodeType::PIDControllerAdvanced);

    QMenu* propertyMenu = addMenu->addMenu("Property");
    addEntry(propertyMenu, "Dropdown",        NodeType::PropertyDropdown);
    addEntry(propertyMenu, "Number",          NodeType::PropertyNumber);
    addEntry(propertyMenu, "Slider",          NodeType::PropertySlider);
    addEntry(propertyMenu, "Toggle",          NodeType::PropertyToggle);
    addEntry(propertyMenu, "Text",            NodeType::PropertyText);
    propertyMenu->addSeparator();
    addEntry(propertyMenu, "Tooltip Number",  NodeType::TooltipNumber);
    addEntry(propertyMenu, "Tooltip On/Off",  NodeType::TooltipOnOff);

    QMenu* funcMenu = addMenu->addMenu("Function");
    addEntry(funcMenu, "Function (1 input)",      NodeType::Function1);
    addEntry(funcMenu, "Function (3 inputs)",     NodeType::Function3);
    addEntry(funcMenu, "Function (8 inputs)",     NodeType::Function8);
    addEntry(funcMenu, "Bool Function (4 inputs)", NodeType::FunctionBool4);
    addEntry(funcMenu, "Bool Function (8 inputs)", NodeType::FunctionBool8);

    menu.exec(mapToGlobal(viewPos));
}

// ─────────────────────────────────────────────────────────────────────────────
// mousePressEvent
// ─────────────────────────────────────────────────────────────────────────────
void NodeGraphView::mousePressEvent(QMouseEvent* event)
{
    if (event->button() == Qt::MiddleButton) {
        panning_ = true;
        lastMousePos_ = event->pos();
        setCursor(Qt::ClosedHandCursor);
        event->accept();
        return;
    }

    if (event->button() == Qt::RightButton) {
        QPointF scenePos = mapToScene(event->pos());
        showContextMenu(event->pos(), scenePos);
        event->accept();
        return;
    }

    if (event->button() == Qt::LeftButton && mc_) {
        QPointF scenePos = mapToScene(event->pos());

        NodeGraphItem* item = nullptr;
        int portIdx = -1;
        bool isOutput = false;

        if (findPortAt(scenePos, item, portIdx, isOutput)) {
            if (isOutput) {
                dragging_ = true;
                dragColor_ = item->portColorAt(portIdx, true);
                dragFixedEndScene_ = item->outputPortScenePos(portIdx);
                dragFixedIsOutput_ = true;
                dragFixedNodeId_   = item->nodeId();
                dragFixedPortIdx_  = portIdx;
            } else {
                ConnectionItem* existing = nullptr;
                for (auto* w : connectionItems_) {
                    if (w->targetNodeId() == item->nodeId() && w->targetPortIdx() == portIdx) {
                        existing = w;
                        break;
                    }
                }
                if (!existing) {
                    event->accept();
                    return;
                }

                int srcNodeId  = existing->sourceNodeId();
                int srcPortIdx = existing->sourcePortIdx();
                auto srcIt = nodeItems_.find(srcNodeId);

                removeConnectionToInput(item->nodeId(), portIdx);

                if (srcIt != nodeItems_.end()) {
                    dragging_ = true;
                    dragColor_ = srcIt->second->portColorAt(srcPortIdx, true);
                    dragFixedEndScene_ = srcIt->second->outputPortScenePos(srcPortIdx);
                    dragFixedIsOutput_ = true;
                    dragFixedNodeId_   = srcNodeId;
                    dragFixedPortIdx_  = srcPortIdx;
                }
            }

            if (dragging_) {
                dragPreview_ = new QGraphicsPathItem();
                dragPreview_->setPen(QPen(dragColor_, 2, Qt::DashLine, Qt::RoundCap));
                dragPreview_->setZValue(10);
                scene_->addItem(dragPreview_);
                updateDragPreview(scenePos);
            }

            event->accept();
            return;
        }

        // Track which node (if any) was clicked, for Delete-key support
        NodeGraphItem* clickedNode = findNodeAt(scenePos);
        selectedNodeId_ = clickedNode ? clickedNode->nodeId() : -1;
    }

    QGraphicsView::mousePressEvent(event);
}

// ─────────────────────────────────────────────────────────────────────────────
// mouseMoveEvent
// ─────────────────────────────────────────────────────────────────────────────
void NodeGraphView::mouseMoveEvent(QMouseEvent* event)
{
    if (dragging_) {
        updateDragPreview(mapToScene(event->pos()));
        event->accept();
        return;
    }

    if (panning_) {
        QPoint delta = event->pos() - lastMousePos_;
        lastMousePos_ = event->pos();
        horizontalScrollBar()->setValue(horizontalScrollBar()->value() - delta.x());
        verticalScrollBar()->setValue  (verticalScrollBar()->value()   - delta.y());
        event->accept();
        return;
    }

    QGraphicsView::mouseMoveEvent(event);
}

// ─────────────────────────────────────────────────────────────────────────────
// mouseReleaseEvent
//
// In addition to finishing wire drags, this also snaps the just-dragged node
// (if any) to the grid once the mouse button is released.
// ─────────────────────────────────────────────────────────────────────────────
void NodeGraphView::mouseReleaseEvent(QMouseEvent* event)
{
    if (event->button() == Qt::MiddleButton) {
        panning_ = false;
        setCursor(Qt::ArrowCursor);
        event->accept();
        return;
    }

    if (event->button() == Qt::LeftButton && dragging_) {
        QPointF scenePos = mapToScene(event->pos());

        NodeGraphItem* item = nullptr;
        int portIdx = -1;
        bool isOutput = false;

        if (findPortAt(scenePos, item, portIdx, isOutput)) {
            if (!isOutput) {
                QColor targetColor = item->portColorAt(portIdx, false);
                if (targetColor == dragColor_) {
                    removeConnectionToInput(item->nodeId(), portIdx);
                    createConnection(dragFixedNodeId_, dragFixedPortIdx_,
                                    item->nodeId(), portIdx);
                    endDrag();
                    event->accept();
                    return;
                }
            }
            // Dropped on an incompatible port (e.g. number -> bool) or on an
            // output: reject the connection and cancel the drag.
            endDrag();
            event->accept();
            return;
        }

        endDrag();
        event->accept();
        return;
    }

    // If we get here and a node was selected/dragged via the base class,
    // snap it to the grid now that the drag has finished.
    if (event->button() == Qt::LeftButton && selectedNodeId_ >= 0) {
        auto it = nodeItems_.find(selectedNodeId_);
        if (it != nodeItems_.end()) {
            NodeGraphItem* item = it->second;
            QPointF snapped = snapToGrid(item->pos());
            item->setPos(snapped);
            // itemChange() already keeps Node::posX/posY in sync on move,
            // so no extra bookkeeping needed here beyond the visual snap.
        }
    }

    QGraphicsView::mouseReleaseEvent(event);
}

// ─────────────────────────────────────────────────────────────────────────────
// keyPressEvent — Delete key removes the currently selected node
// ─────────────────────────────────────────────────────────────────────────────
void NodeGraphView::keyPressEvent(QKeyEvent* event)
{
    if ((event->key() == Qt::Key_Delete || event->key() == Qt::Key_Backspace) &&
        selectedNodeId_ >= 0) {
        deleteNode(selectedNodeId_);
        event->accept();
        return;
    }
    QGraphicsView::keyPressEvent(event);
}

// ─────────────────────────────────────────────────────────────────────────────
// wheelEvent
// ─────────────────────────────────────────────────────────────────────────────
void NodeGraphView::wheelEvent(QWheelEvent* event)
{
    constexpr double kZoomFactor = 1.15;
    constexpr double kMaxScale   = 4.0;

    // Zooming out stops once the whole board fits in the window
    const QRectF board = boardRect();
    const double minScale = std::min(viewport()->width()  / board.width(),
                                     viewport()->height() / board.height());

    const double current = transform().m11();
    double factor = (event->angleDelta().y() > 0) ? kZoomFactor : 1.0 / kZoomFactor;
    const double target = std::clamp(current * factor, minScale, kMaxScale);
    factor = target / current;
    if (factor != 1.0)
        scale(factor, factor);
}
