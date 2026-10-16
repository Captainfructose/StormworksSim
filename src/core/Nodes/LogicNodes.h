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

// ── CONSTANT ON ───────────────────────────────────────────────────────────────
// No inputs; always outputs true.
class ConstantOnNode : public Node {
public:
    ConstantOnNode() : Node(NodeType::ConstantOn, "Constant On") {
        outputs = { Port::makeBool("Out") };
    }
    void evaluate() override { outputBool(0, true); }
};

// ── PUSH TO TOGGLE ────────────────────────────────────────────────────────────
// in[0]=In → out[0]=Out. Each time the input turns on (rising edge) the output
// flips; it keeps its state otherwise. Type 28 in Stormworks XML.
class PushToToggleNode : public Node {
public:
    PushToToggleNode() : Node(NodeType::PushToToggle, "Push to Toggle") {
        inputs  = { Port::makeBool("In") };
        outputs = { Port::makeBool("Out") };
    }
    void evaluate() override {
        const bool in = inputBool(0);
        if (in && !prev_) state_ = !state_;
        prev_ = in;
        outputBool(0, state_);
    }
private:
    bool prev_  = false;
    bool state_ = false;
};

// ── TOGGLE TO PUSH ────────────────────────────────────────────────────────────
// in[0]=In → out[0]=Out. Outputs on for a single tick whenever the input
// changes state (either edge). Type 48 in Stormworks XML.
class ToggleToPushNode : public Node {
public:
    ToggleToPushNode() : Node(NodeType::ToggleToPush, "Toggle to Push") {
        inputs  = { Port::makeBool("In") };
        outputs = { Port::makeBool("Out") };
    }
    void evaluate() override {
        const bool in = inputBool(0);
        outputBool(0, in != prev_);
        prev_ = in;
    }
private:
    bool prev_ = false;
};

// ── SR LATCH ──────────────────────────────────────────────────────────────────
// in[0]=Set, in[1]=Reset → out[0]=Q, out[1]=NOT Q. Holds its state when
// neither is on. Set takes priority if both are on. Type 24 in Stormworks XML.
class SRLatchNode : public Node {
public:
    SRLatchNode() : Node(NodeType::SRLatch, "SR Latch") {
        inputs  = { Port::makeBool("Set"), Port::makeBool("Reset") };
        outputs = { Port::makeBool("Q"), Port::makeBool("!Q") };
    }
    void evaluate() override {
        if (inputBool(0))      state_ = true;
        else if (inputBool(1)) state_ = false;
        outputBool(0, state_);
        outputBool(1, !state_);
    }
private:
    bool state_ = false;
};

// ── JK FLIP-FLOP ──────────────────────────────────────────────────────────────
// in[0]=J, in[1]=K → out[0]=Q, out[1]=NOT Q.
// J only sets, K only resets, both on toggles every tick, neither holds.
// Type 25 in Stormworks XML.
class JKFlipFlopNode : public Node {
public:
    JKFlipFlopNode() : Node(NodeType::JKFlipFlop, "JK Flip-Flop") {
        inputs  = { Port::makeBool("J"), Port::makeBool("K") };
        outputs = { Port::makeBool("Q"), Port::makeBool("!Q") };
    }
    void evaluate() override {
        const bool j = inputBool(0);
        const bool k = inputBool(1);
        if (j && k)       state_ = !state_;
        else if (j)       state_ = true;
        else if (k)       state_ = false;
        outputBool(0, state_);
        outputBool(1, !state_);
    }
private:
    bool state_ = false;
};
