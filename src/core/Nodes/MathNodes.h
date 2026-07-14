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
// Out = clamp(Value, minBound, maxBound) — type 11
// minBound/maxBound configured via settings dialog
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
// Out = |Value| — type 14
class AbsNode : public Node {
public:
    AbsNode() : Node(NodeType::Abs, "Abs") {
        inputs  = { Port::makeNumber("Value") };
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
