#pragma once

#include "../Node.h"
#include <cmath>
#include <algorithm>

// ─────────────────────────────────────────────────────────────────────────────
// Math Nodes
//
// All math nodes operate on Number ports.
// Division by zero returns 0 (Stormworks behaviour).
// ─────────────────────────────────────────────────────────────────────────────

// ── ADD ───────────────────────────────────────────────────────────────────────
// Out = A + B
class AddNode : public Node {
public:
    AddNode() : Node(NodeType::Add, "Add") {
        inputs  = { Port::makeNumber("A"), Port::makeNumber("B") };
        outputs = { Port::makeNumber("Out") };
    }
    void evaluate() override {
        outputNumber(0, inputNumber(0) + inputNumber(1));
    }
};

// ── SUBTRACT ─────────────────────────────────────────────────────────────────
// Out = A - B
class SubtractNode : public Node {
public:
    SubtractNode() : Node(NodeType::Subtract, "Subtract") {
        inputs  = { Port::makeNumber("A"), Port::makeNumber("B") };
        outputs = { Port::makeNumber("Out") };
    }
    void evaluate() override {
        outputNumber(0, inputNumber(0) - inputNumber(1));
    }
};

// ── MULTIPLY ─────────────────────────────────────────────────────────────────
// Out = A × B
class MultiplyNode : public Node {
public:
    MultiplyNode() : Node(NodeType::Multiply, "Multiply") {
        inputs  = { Port::makeNumber("A"), Port::makeNumber("B") };
        outputs = { Port::makeNumber("Out") };
    }
    void evaluate() override {
        outputNumber(0, inputNumber(0) * inputNumber(1));
    }
};

// ── DIVIDE ───────────────────────────────────────────────────────────────────
// Out = A / B  (returns 0 if B == 0, matching Stormworks)
// IsError = true when B == 0 (divide-by-zero detected), matching the real
// Stormworks Divide node's second output.
class DivideNode : public Node {
public:
    DivideNode() : Node(NodeType::Divide, "Divide") {
        inputs  = { Port::makeNumber("A"), Port::makeNumber("B") };
        outputs = { Port::makeNumber("Out"), Port::makeBool("IsError") };
    }
    void evaluate() override {
        double b = inputNumber(1);
        bool   divByZero = (b == 0.0);
        outputNumber(0, divByZero ? 0.0 : (inputNumber(0) / b));
        outputBool(1, divByZero);
    }
};

// ── CLAMP ─────────────────────────────────────────────────────────────────────
// Out = clamp(Value, minBound, maxBound)
// minBound/maxBound are configured via the node's settings dialog rather
// than as input ports, matching Stormworks' actual Clamp node.
class ClampNode : public Node {
public:
    double minBound = 0.0;
    double maxBound = 1.0;

    ClampNode() : Node(NodeType::Clamp, "Clamp") {
        inputs  = { Port::makeNumber("Value") };
        outputs = { Port::makeNumber("Out") };
    }
    void evaluate() override {
        outputNumber(0, std::clamp(inputNumber(0), minBound, maxBound));
    }
};

// ── ABS ───────────────────────────────────────────────────────────────────────
// Out = |Value|
class AbsNode : public Node {
public:
    AbsNode() : Node(NodeType::Abs, "Abs") {
        inputs  = { Port::makeNumber("Value") };
        outputs = { Port::makeNumber("Out") };
    }
    void evaluate() override {
        outputNumber(0, std::abs(inputNumber(0)));
    }
};

// ── MIN ───────────────────────────────────────────────────────────────────────
// Out = min(A, B)
class MinNode : public Node {
public:
    MinNode() : Node(NodeType::Min, "Min") {
        inputs  = { Port::makeNumber("A"), Port::makeNumber("B") };
        outputs = { Port::makeNumber("Out") };
    }
    void evaluate() override {
        outputNumber(0, std::min(inputNumber(0), inputNumber(1)));
    }
};

// ── MAX ───────────────────────────────────────────────────────────────────────
// Out = max(A, B)
class MaxNode : public Node {
public:
    MaxNode() : Node(NodeType::Max, "Max") {
        inputs  = { Port::makeNumber("A"), Port::makeNumber("B") };
        outputs = { Port::makeNumber("Out") };
    }
    void evaluate() override {
        outputNumber(0, std::max(inputNumber(0), inputNumber(1)));
    }
};

// ── MODULO ───────────────────────────────────────────────────────────────────
// Out = A mod B  (returns 0 if B == 0, matching Stormworks)
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

// ── GREATER THAN ──────────────────────────────────────────────────────────────
// Out = (A > B), a bool result from two number inputs
class GreaterThanNode : public Node {
public:
    GreaterThanNode() : Node(NodeType::GreaterThan, "Greater Than") {
        inputs  = { Port::makeNumber("A"), Port::makeNumber("B") };
        outputs = { Port::makeBool("Out") };
    }
    void evaluate() override {
        outputBool(0, inputNumber(0) > inputNumber(1));
    }
};

// ── LESS THAN ─────────────────────────────────────────────────────────────────
// Out = (A < B), a bool result from two number inputs
class LessThanNode : public Node {
public:
    LessThanNode() : Node(NodeType::LessThan, "Less Than") {
        inputs  = { Port::makeNumber("A"), Port::makeNumber("B") };
        outputs = { Port::makeBool("Out") };
    }
    void evaluate() override {
        outputBool(0, inputNumber(0) < inputNumber(1));
    }
};

// ── EQUAL TO ──────────────────────────────────────────────────────────────────
// Out = true if |A - B| <= tolerance, a bool result from two number inputs.
// Floating point equality needs a tolerance rather than exact comparison;
// tolerance defaults to 0.0001 to match Stormworks' real default, and is
// configurable via the node's settings dialog.
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
