#pragma once

#include "../Node.h"
#include <algorithm>
#include <cmath>

// ── ADD ───────────────────────────────────────────────────────────────────────
class AddNode : public Node {
public:
    AddNode() : Node(NodeType::Add, "Add") {
        inputs  = { Port::makeNumber("A"), Port::makeNumber("B") };
        outputs = { Port::makeNumber("Out") };
    }
    void evaluate() override { outputNumber(0, inputNumber(0) + inputNumber(1)); }
};

// ── SUBTRACT ──────────────────────────────────────────────────────────────────
class SubtractNode : public Node {
public:
    SubtractNode() : Node(NodeType::Subtract, "Subtract") {
        inputs  = { Port::makeNumber("A"), Port::makeNumber("B") };
        outputs = { Port::makeNumber("Out") };
    }
    void evaluate() override { outputNumber(0, inputNumber(0) - inputNumber(1)); }
};

// ── MULTIPLY ──────────────────────────────────────────────────────────────────
class MultiplyNode : public Node {
public:
    MultiplyNode() : Node(NodeType::Multiply, "Multiply") {
        inputs  = { Port::makeNumber("A"), Port::makeNumber("B") };
        outputs = { Port::makeNumber("Out") };
    }
    void evaluate() override { outputNumber(0, inputNumber(0) * inputNumber(1)); }
};

// ── DIVIDE ────────────────────────────────────────────────────────────────────
// Out = A / B  (returns 0 if B == 0)
// IsError = true when B == 0 (divide-by-zero)
class DivideNode : public Node {
public:
    DivideNode() : Node(NodeType::Divide, "Divide") {
        inputs  = { Port::makeNumber("A"), Port::makeNumber("B") };
        outputs = { Port::makeNumber("Out"), Port::makeBool("Err") };
    }
    void evaluate() override {
        double b = inputNumber(1);
        bool   divByZero = (b == 0.0);
        outputNumber(0, divByZero ? 0.0 : (inputNumber(0) / b));
        outputBool(1, divByZero);
    }
};

// ── CLAMP ─────────────────────────────────────────────────────────────────────
// Out = clamp(Value, minBound, maxBound) — type 11
// minBound/maxBound configured via settings dialog
class ClampNode : public Node {
public:
    double minBound = 0.0;
    double maxBound = 1.0;

    ClampNode() : Node(NodeType::Clamp, "Clamp") {
        inputs  = { Port::makeNumber("A") };
        outputs = { Port::makeNumber("Out") };
    }
    void evaluate() override {
        outputNumber(0, std::clamp(inputNumber(0), minBound, maxBound));
    }
};

// ── ABS ───────────────────────────────────────────────────────────────────────
// Out = |Value| — type 14
class AbsNode : public Node {
public:
    AbsNode() : Node(NodeType::Abs, "Abs") {
        inputs  = { Port::makeNumber("A") };
        outputs = { Port::makeNumber("Out") };
    }
    void evaluate() override { outputNumber(0, std::abs(inputNumber(0))); }
};

// ── MODULO ────────────────────────────────────────────────────────────────────
// Out = A mod B — type 38
class ModuloNode : public Node {
public:
    ModuloNode() : Node(NodeType::Modulo, "Modulo") {
        inputs  = { Port::makeNumber("A"), Port::makeNumber("B") };
        outputs = { Port::makeNumber("Out") };
    }
    void evaluate() override {
        double b = inputNumber(1);
        outputNumber(0, (b != 0.0) ? std::fmod(inputNumber(0), b) : 0.0);
    }
};

// ── THRESHOLD ─────────────────────────────────────────────────────────────────
// Out = true while minBound <= Value <= maxBound. Type 12 in Stormworks XML
// (<min text="X"/> and <max text="X" value="X"/>); new nodes default to 0..1.
class ThresholdNode : public Node {
public:
    double minBound = 0.0;
    double maxBound = 1.0;

    ThresholdNode() : Node(NodeType::Threshold, "Threshold") {
        inputs  = { Port::makeNumber("A") };
        outputs = { Port::makeBool("Out") };
    }
    void evaluate() override {
        const double v = inputNumber(0);
        outputBool(0, v >= minBound && v <= maxBound);
    }
};

// ── NUMERICAL SWITCHBOX ───────────────────────────────────────────────────────
// in[0]=On value, in[1]=Off value, in[2]=Switch(bool) → out[0]=Out.
// Outputs the On value while Switch is on, otherwise the Off value.
// Type 22 in Stormworks XML.
class NumericalSwitchboxNode : public Node {
public:
    NumericalSwitchboxNode() : Node(NodeType::NumericalSwitchbox, "Switchbox") {
        inputs  = { Port::makeNumber("On"), Port::makeNumber("Off"), Port::makeBool("Switch") };
        outputs = { Port::makeNumber("Out") };
    }
    void evaluate() override {
        outputNumber(0, inputBool(2) ? inputNumber(0) : inputNumber(1));
    }
};

// ── NUMERICAL JUNCTION ────────────────────────────────────────────────────────
// in[0]=Value, in[1]=Switch(bool) → out[0]=On path, out[1]=Off path.
// The opposite of a switchbox: sends the value down the On path while Switch is
// on and down the Off path otherwise; the path not chosen outputs 0.
// Type 21 in Stormworks XML. Input order (Value in1, Switch in2) and output order
// (On = first, Off = second via node_index="1") confirmed against a wired sample.
// That the unchosen path outputs 0 is an assumption.
class NumericalJunctionNode : public Node {
public:
    NumericalJunctionNode() : Node(NodeType::NumericalJunction, "Junction") {
        inputs  = { Port::makeNumber("A"), Port::makeBool("Switch") };
        outputs = { Port::makeNumber("On"), Port::makeNumber("Off") };
    }
    void evaluate() override {
        const double v  = inputNumber(0);
        const bool   on = inputBool(1);
        outputNumber(0, on ? v   : 0.0);
        outputNumber(1, on ? 0.0 : v);
    }
};

// ── DELTA ─────────────────────────────────────────────────────────────────────
// Out = change in the input since the previous tick (current - previous).
// The previous value starts at 0, so the first tick outputs the input itself.
class DeltaNode : public Node {
public:
    DeltaNode() : Node(NodeType::Delta, "Delta") {
        inputs  = { Port::makeNumber("A") };
        outputs = { Port::makeNumber("Out") };
    }
    void evaluate() override {
        double current = inputNumber(0);
        outputNumber(0, current - previous_);
        previous_ = current;
    }
private:
    double previous_ = 0.0;
};

// ── GREATER THAN ─────────────────────────────────────────────────────────────
// Out = (A > B) — type 17
class GreaterThanNode : public Node {
public:
    GreaterThanNode() : Node(NodeType::GreaterThan, "Greater Than") {
        inputs  = { Port::makeNumber("A"), Port::makeNumber("B") };
        outputs = { Port::makeBool("Out") };
    }
    void evaluate() override { outputBool(0, inputNumber(0) > inputNumber(1)); }
};

// ── LESS THAN ────────────────────────────────────────────────────────────────
// Out = (A < B) — type 18
class LessThanNode : public Node {
public:
    LessThanNode() : Node(NodeType::LessThan, "Less Than") {
        inputs  = { Port::makeNumber("A"), Port::makeNumber("B") };
        outputs = { Port::makeBool("Out") };
    }
    void evaluate() override { outputBool(0, inputNumber(0) < inputNumber(1)); }
};

// ── EQUAL TO ─────────────────────────────────────────────────────────────────
// Out = |A - B| <= tolerance — type 42
class EqualToNode : public Node {
public:
    double tolerance = 0.0001;

    EqualToNode() : Node(NodeType::EqualTo, "Equal To") {
        inputs  = { Port::makeNumber("A"), Port::makeNumber("B") };
        outputs = { Port::makeBool("Out") };
    }
    void evaluate() override {
        outputBool(0, std::abs(inputNumber(0) - inputNumber(1)) <= tolerance);
    }
};
