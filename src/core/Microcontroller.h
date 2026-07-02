#pragma once

#include "Node.h"
#include "Connection.h"

#include <QString>
#include <QDomDocument>
#include <vector>
#include <unordered_map>
#include <memory>

// ─────────────────────────────────────────────────────────────────────────────
// Microcontroller
//
// Owns the full node graph:
//   • nodes_       — all Node instances keyed by their integer ID
//   • connections_ — all wires between ports
//   • evalOrder_   — topologically sorted node IDs (computed once after load)
//
// Each call to tick() does exactly one Stormworks simulation step:
//   1. Propagate I/O input values (sliders/buttons) to BoolInput/NumberInput nodes
//   2. Walk evalOrder_ and call Node::evaluate() on each node in dependency order
//   3. Output values are then readable from BoolOutput/NumberOutput nodes
// ─────────────────────────────────────────────────────────────────────────────
class Microcontroller {
public:
    Microcontroller() = default;

    // ── Graph construction (used when building from the GUI or tests) ─────────

    // Add a node; assigns a unique id and takes ownership. Returns the id.
    int  addNode(std::unique_ptr<Node> node);

    // Add a directed wire between ports. Duplicate wires are silently ignored.
    void addConnection(const Connection& conn);

    // Remove whatever connection currently feeds the given input port.
    // Safe to call even if no connection exists (no-op in that case).
    // Used by the GUI when the user detaches or rewires an input.
    void removeConnectionToInput(int targetNodeId, int targetPortIdx);

    // Remove a node entirely along with every connection touching it
    // (as either source or target). Safe to call with an unknown id (no-op).
    void removeNode(int nodeId);

    // Remove everything — resets to an empty graph
    void clear();

    // ── Stormworks XML import ─────────────────────────────────────────────────

    // Load a microcontroller from a Stormworks .xml save file.
    // Returns true on success, false with errorMsg set on failure.
    bool loadFromFile(const QString& path, QString& errorMsg);

    // ── Stormworks XML export ─────────────────────────────────────────────────

    // Save back to the currently loaded file path (the one passed to the most
    // recent loadFromFile call). Fails if no file has been loaded yet.
    bool saveToFile(QString& errorMsg);

    // Save to an explicit path, updating the "current file" so a later plain
    // saveToFile() call writes here too.
    bool saveToFileAs(const QString& path, QString& errorMsg);

    // True once a file has been loaded or successfully saved-as, meaning
    // saveToFile() has a valid target path to write to.
    bool hasFilePath() const { return !currentFilePath_.isEmpty(); }
    QString currentFilePath() const { return currentFilePath_; }

    // ── Simulation ────────────────────────────────────────────────────────────

    // Advance the simulation by one tick.
    // Reads from input nodes, evaluates all nodes in topological order,
    // and writes results to output nodes.
    void tick();

    // ── Accessors ─────────────────────────────────────────────────────────────

    // Raw node map — GUI uses this to build NodeGraphItems
    const std::unordered_map<int, std::unique_ptr<Node>>& nodes() const { return nodes_; }

    // Wire list — GUI uses this to draw ConnectionItems
    const std::vector<Connection>& connections() const { return connections_; }

    // Retrieve a single node by id (returns nullptr if not found)
    Node* node(int id) const;

    // Current simulation tick counter (incremented each tick() call)
    int tickCount() const { return tickCount_; }

private:
    // ── Internal state ────────────────────────────────────────────────────────

    std::unordered_map<int, std::unique_ptr<Node>> nodes_;
    std::vector<Connection> connections_;

    // Topologically sorted node IDs — computed by buildEvalOrder()
    std::vector<int> evalOrder_;

    int nextId_    = 0; // auto-increment for addNode()
    int tickCount_ = 0;

    // The full original XML document as loaded — preserved so we can rewrite
    // only the node/connection sections at save time while keeping everything
    // else (vehicle metadata, unknown elements, etc.) byte-for-byte original.
    QDomDocument originalDoc_;
    QString      currentFilePath_;

    // Node ids that existed in the originally loaded file's <components>
    // section (logic/math/composite — NOT I/O bridge nodes). Used at save
    // time to distinguish "edit existing" from "newly added, needs a new
    // DOM element" for the node types we support adding/removing.
    std::unordered_map<int, bool> originalComponentIds_;

    // Node ids that existed in components_bridge (I/O nodes) in the original
    // file. Per current design, I/O nodes can only be edited in place on
    // save — adding or removing them isn't written back, since that would
    // require synthesizing/removing matching <nodes> (vehicle edge) entries
    // we don't fully model.
    std::unordered_map<int, bool> originalBridgeIds_;

    // ── Helpers ───────────────────────────────────────────────────────────────

    // Kahn's algorithm topological sort — called after loading or editing
    void buildEvalOrder();

    // Push output values across all wires to downstream input ports
    void propagateValues();

    // XML parsing helpers
    bool parseXmlDocument(const QDomDocument& doc, QString& errorMsg);
    NodeType nodeTypeFromComponentType(int id, bool& ok) const;

    // XML export helpers
    // Maps our NodeType back to Stormworks' <components> type id.
    // Returns -1 for types that don't have a direct component representation
    // (e.g. our I/O node types, which live in components_bridge instead).
    int componentTypeFromNodeType(NodeType type) const;

    // Rebuilds the <nodes>, <group><components_bridge>, and
    // <group><components> sections of doc from the current nodes_/connections_
    // state. All node positions are written as 0,0 per current requirements.
    void rebuildXmlSections(QDomDocument& doc) const;
};
