#pragma once

// ─────────────────────────────────────────────────────────────────────────────
// Connection — a directed wire between one output port and one input port
//
// In Stormworks, a wire always goes:
//   source node [output port]  →  target node [input port]
//
// Port indices are 0-based and match the order in Node::inputs / Node::outputs.
// ─────────────────────────────────────────────────────────────────────────────
struct Connection {
    int sourceNodeId;  // ID of the node that produces the value
    int sourcePortIdx; // which output port on the source node

    int targetNodeId;  // ID of the node that consumes the value
    int targetPortIdx; // which input port on the target node
};
