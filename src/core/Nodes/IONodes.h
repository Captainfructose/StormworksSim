#pragma once

#include "../Node.h"

// ─────────────────────────────────────────────────────────────────────────────
// I/O Nodes
// ─────────────────────────────────────────────────────────────────────────────

// ── BOOL INPUT ────────────────────────────────────────────────────────────────
class BoolInputNode : public Node {
public:
    int channelIndex = 0;
    BoolInputNode() : Node(NodeType::BoolInput, "Bool Input") {
        inputs  = { Port::makeBool("Value") };
        outputs = { Port::makeBool("Out") };
    }
    void evaluate() override { outputBool(0, inputBool(0)); }
};

// ── BOOL OUTPUT ───────────────────────────────────────────────────────────────
class BoolOutputNode : public Node {
public:
    int channelIndex = 0;
    BoolOutputNode() : Node(NodeType::BoolOutput, "Bool Output") {
        inputs  = { Port::makeBool("In") };
        outputs = { Port::makeBool("Value") };
    }
    void evaluate() override { outputBool(0, inputBool(0)); }
};

// ── NUMBER INPUT ──────────────────────────────────────────────────────────────
// stepSize, minValue, maxValue are set via the node settings dialog and
// read by IOPanel to configure the slider and spinbox accordingly.
class NumberInputNode : public Node {
public:
    int    channelIndex = 0;
    double stepSize     = 0.1;      // slider/spinbox step increment
    double minValue     = -1000.0;  // slider minimum
    double maxValue     =  1000.0;  // slider maximum

    NumberInputNode() : Node(NodeType::NumberInput, "Number Input") {
        inputs  = { Port::makeNumber("Value") };
        outputs = { Port::makeNumber("Out") };
    }
    void evaluate() override { outputNumber(0, inputNumber(0)); }
};

// ── NUMBER OUTPUT ─────────────────────────────────────────────────────────────
class NumberOutputNode : public Node {
public:
    int channelIndex = 0;
    NumberOutputNode() : Node(NodeType::NumberOutput, "Number Output") {
        inputs  = { Port::makeNumber("In") };
        outputs = { Port::makeNumber("Value") };
    }
    void evaluate() override { outputNumber(0, inputNumber(0)); }
};

// ── NUMBER LITERAL ────────────────────────────────────────────────────────────
// Outputs a constant value every tick. Value is set by XML parser or settings dialog.
class NumberLiteralNode : public Node {
public:
    double constantValue = 0.0;

    NumberLiteralNode() : Node(NodeType::NumberLiteral, "Constant Number") {
        outputs = { Port::makeNumber("Value") };
    }
    void evaluate() override {
        outputNumber(0, constantValue);
    }
};

// ── PROPERTY DROPDOWN ─────────────────────────────────────────────────────────
// A named list of label/value options; outputs the value of the selected one.
// Type 20 in Stormworks XML: name="..." and i="<selected index>" on <object>,
// options under <items><i l="label"><v text="X" value="X"/></i>...</items>.
class PropertyDropdownNode : public Node {
public:
    struct Item {
        QString label;
        double  value = 0.0;
    };

    QString           propertyName = "value";
    std::vector<Item> items        = { { "option 1", 0.0 }, { "option 2", 0.0 } };
    int               selected     = 0;

    PropertyDropdownNode() : Node(NodeType::PropertyDropdown, "Property Dropdown") {
        outputs = { Port::makeNumber("Value") };
    }
    double selectedValue() const {
        return (selected >= 0 && selected < (int)items.size()) ? items[selected].value : 0.0;
    }
    void evaluate() override { outputNumber(0, selectedValue()); }
};
