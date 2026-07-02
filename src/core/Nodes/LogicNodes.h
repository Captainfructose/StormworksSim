#pragma once

#include "../Node.h"

// ─────────────────────────────────────────────────────────────────────────────
// Logic Nodes
//
// All logic nodes operate on Bool ports only and mirror Stormworks behaviour
// exactly: inputs default to false (0) when disconnected.
// ─────────────────────────────────────────────────────────────────────────────

// ── AND ───────────────────────────────────────────────────────────────────────
// Output is true only when ALL inputs are true
class AndNode : public Node {
public:
    AndNode() : Node(NodeType::And, "AND") {
        inputs  = { Port::makeBool("A"), Port::makeBool("B") };
        outputs = { Port::makeBool("Out") };
    }
    void evaluate() override {
        outputBool(0, inputBool(0) && inputBool(1));
    }
};

// ── OR ────────────────────────────────────────────────────────────────────────
// Output is true when ANY input is true
class OrNode : public Node {
public:
    OrNode() : Node(NodeType::Or, "OR") {
        inputs  = { Port::makeBool("A"), Port::makeBool("B") };
        outputs = { Port::makeBool("Out") };
    }
    void evaluate() override {
        outputBool(0, inputBool(0) || inputBool(1));
    }
};

// ── NOT ───────────────────────────────────────────────────────────────────────
// Output is the logical inverse of the input
class NotNode : public Node {
public:
    NotNode() : Node(NodeType::Not, "NOT") {
        inputs  = { Port::makeBool("In") };
        outputs = { Port::makeBool("Out") };
    }
    void evaluate() override {
        outputBool(0, !inputBool(0));
    }
};

// ── XOR ───────────────────────────────────────────────────────────────────────
// Output is true when inputs DIFFER (exclusive or)
class XorNode : public Node {
public:
    XorNode() : Node(NodeType::Xor, "XOR") {
        inputs  = { Port::makeBool("A"), Port::makeBool("B") };
        outputs = { Port::makeBool("Out") };
    }
    void evaluate() override {
        outputBool(0, inputBool(0) != inputBool(1));
    }
};

// ── NAND ──────────────────────────────────────────────────────────────────────
// Output is false only when ALL inputs are true (NOT AND)
class NandNode : public Node {
public:
    NandNode() : Node(NodeType::Nand, "NAND") {
        inputs  = { Port::makeBool("A"), Port::makeBool("B") };
        outputs = { Port::makeBool("Out") };
    }
    void evaluate() override {
        outputBool(0, !(inputBool(0) && inputBool(1)));
    }
};

// ── NOR ───────────────────────────────────────────────────────────────────────
// Output is true only when ALL inputs are false (NOT OR)
class NorNode : public Node {
public:
    NorNode() : Node(NodeType::Nor, "NOR") {
        inputs  = { Port::makeBool("A"), Port::makeBool("B") };
        outputs = { Port::makeBool("Out") };
    }
    void evaluate() override {
        outputBool(0, !(inputBool(0) || inputBool(1)));
    }
};
