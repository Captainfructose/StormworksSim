#pragma once

#include "../Node.h"
#include <vector>

// ─────────────────────────────────────────────────────────────────────────────
// Composite Nodes
// ─────────────────────────────────────────────────────────────────────────────

// ── COMPOSITE CHANNEL INFO ────────────────────────────────────────────────────
// Describes one channel inside a CompositeInput node.
struct CompositeChannel {
    int      index    = 1;              // which composite slot (1-32)
    PortType dataType = PortType::Number; // bool or number
};

// ── COMPOSITE INPUT (type 4 in components_bridge) ─────────────────────────────
// Represents a composite wire coming into the MC from the vehicle.
// Has one purple output port. The settings dialog configures how many
// channels it contains and what type/index each channel is.
// Downstream Composite Read nodes extract individual channels.
class CompositeInputNode : public Node {
public:
    std::vector<CompositeChannel> channels; // per-channel config

    CompositeInputNode() : Node(NodeType::CompositeInput, "Composite Input") {
        // Default: one number channel at index 1
        channels = { { 1, PortType::Number } };
        outputs  = { Port::makeNumber("Composite") };
    }

    // Called by settings dialog to reconfigure channels.
    // We keep a single output port — the composite wire.
    void setChannels(const std::vector<CompositeChannel>& ch) {
        channels = ch;
        // Keep a single composite output port regardless of channel count
        outputs = { Port::makeNumber("Composite") };
    }

    void evaluate() override {
        // The composite input acts as a multi-channel source.
        // Individual channel values are stored in inputs[] by the IOPanel.
        // Downstream Composite Read nodes connect to this node and pull
        // from the appropriate input slot via their portIdx connection.
        // We pass input[0] through to output[0] as the "composite wire".
        if (!inputs.empty())
            outputs[0].value = inputs[0].value;
    }
};

// ── COMPOSITE OUTPUT (type 5 in components_bridge) ────────────────────────────
// A composite wire leaving the MC to the vehicle. A pure sink: one purple input
// port, no visible output. Like the other bridge nodes it is only ever edited
// in place when saving (new ones are not written to <components_bridge>).
class CompositeOutputNode : public Node {
public:
    CompositeOutputNode() : Node(NodeType::CompositeOutput, "Composite Output") {
        inputs  = { Port::makeNumber("Composite") };
        outputs = { Port::makeNumber("Value") }; // hidden; mirrors the other sink nodes
    }
    void evaluate() override { outputs[0].value = inputs[0].value; }
};

// ── COMPOSITE PACKER ─────────────────────────────────────────────────────────
class CompositePackerNode : public Node {
public:
    int channelCount = 1;

    CompositePackerNode() : Node(NodeType::CompositePacker, "Comp. Packer") {
        setChannelCount(1);
    }

    void setChannelCount(int count) {
        channelCount = count;
        inputs.clear();
        outputs.clear();
        for (int i = 0; i < count; ++i) {
            inputs.push_back(Port::makeNumber(QString("Ch%1").arg(i + 1)));
            outputs.push_back(Port::makeNumber(QString("Ch%1").arg(i + 1)));
        }
        name = QString("Comp. Packer (%1ch)").arg(count);
    }

    void evaluate() override {
        for (int i = 0; i < (int)inputs.size() && i < (int)outputs.size(); ++i)
            outputs[i].value = inputs[i].value;
    }
};

// ── COMPOSITE READ (NUMBER) ───────────────────────────────────────────────────
class CompositeReadNumberNode : public Node {
public:
    int channelIndex = 1;

    CompositeReadNumberNode() : Node(NodeType::CompositeReadNumber, "Comp. Read (Num)") {
        inputs  = { Port::makeNumber("In") };
        outputs = { Port::makeNumber("Out") };
    }

    void evaluate() override {
        outputNumber(0, inputNumber(0));
    }
};

// ── COMPOSITE READ (BOOL) ─────────────────────────────────────────────────────
class CompositeReadBoolNode : public Node {
public:
    int channelIndex = 1;

    CompositeReadBoolNode() : Node(NodeType::CompositeReadBool, "Comp. Read (Bool)") {
        inputs  = { Port::makeBool("In") };
        outputs = { Port::makeBool("Out") };
    }

    void evaluate() override {
        outputBool(0, inputBool(0));
    }
};

// ── COMPOSITE SWITCHBOX ───────────────────────────────────────────────────────
// in[0]=Composite On, in[1]=Composite Off, in[2]=Switch(bool) → out[0]=Composite.
// Passes through the On composite while Switch is on, otherwise the Off one.
// Type 53 in Stormworks XML. Composite wires are modelled as a single carrier
// value in this simulator (see CompositeWriteNumberNode below).
class CompositeSwitchboxNode : public Node {
public:
    CompositeSwitchboxNode() : Node(NodeType::CompositeSwitchbox, "Comp. Switchbox") {
        inputs  = { Port::makeNumber("Comp. On"), Port::makeNumber("Comp. Off"),
                    Port::makeBool("Switch") };
        outputs = { Port::makeNumber("Composite") };
    }
    void evaluate() override {
        outputs[0].value = inputBool(2) ? inputs[0].value : inputs[1].value;
    }
};

// ── COMPOSITE WRITE (NUMBER) ──────────────────────────────────────────────────
// Port layout:
//   input[0]  = Composite In  (the existing composite wire to merge onto)
//   input[1+] = Channel 1, Channel 2, ... (number-typed, configurable count)
//   output[0] = Composite Out (same composite, passed through — the new
//               channel values conceptually overwrite slots startIndex..
//               startIndex+channelCount-1 in that composite)
//
// channelCount and startIndex are configurable via the settings dialog.
// Bool and Number channels live in separate namespaces within a composite,
// so a Write Number at startIndex=1 doesn't collide with a Write Bool also
// at startIndex=1 on the same wire.
class CompositeWriteNumberNode : public Node {
public:
    int channelCount = 1;
    int startIndex   = 1;

    CompositeWriteNumberNode() : Node(NodeType::CompositeWriteNumber, "Comp. Write (Num)") {
        setChannelCount(1);
    }

    void setChannelCount(int count) {
        channelCount = count;
        rebuildPorts();
    }

    void setStartIndex(int idx) {
        startIndex = idx;
        rebuildPorts(); // refresh channel labels to show their composite index
    }

    void evaluate() override {
        // Pass the composite-in value straight through on the composite-out
        // port. The individual channel input values are conceptually merged
        // into the composite at startIndex..startIndex+channelCount-1; our
        // simplified composite model (a single carrier value) doesn't need
        // to do anything further here since downstream Composite Read nodes
        // read channel values via their own direct connections, same as
        // every other composite node in this simulator.
        outputs[0].value = inputs[0].value;
    }

private:
    void rebuildPorts() {
        inputs.clear();
        inputs.push_back(Port::makeNumber("Composite In"));
        for (int i = 0; i < channelCount; ++i)
            inputs.push_back(Port::makeNumber(QString("Ch%1").arg(startIndex + i)));
        outputs = { Port::makeNumber("Composite Out") };
        name = QString("Comp. Write Num (%1ch @%2)").arg(channelCount).arg(startIndex);
    }
};

// ── COMPOSITE WRITE (BOOL) ────────────────────────────────────────────────────
// Same port layout as CompositeWriteNumberNode, but channel inputs are
// Bool-typed. Writes into the composite's separate bool-channel namespace.
class CompositeWriteBoolNode : public Node {
public:
    int channelCount = 1;
    int startIndex   = 1;

    CompositeWriteBoolNode() : Node(NodeType::CompositeWriteBool, "Comp. Write (Bool)") {
        setChannelCount(1);
    }

    void setChannelCount(int count) {
        channelCount = count;
        rebuildPorts();
    }

    void setStartIndex(int idx) {
        startIndex = idx;
        rebuildPorts();
    }

    void evaluate() override {
        outputs[0].value = inputs[0].value;
    }

private:
    void rebuildPorts() {
        inputs.clear();
        inputs.push_back(Port::makeNumber("Composite In"));
        for (int i = 0; i < channelCount; ++i)
            inputs.push_back(Port::makeBool(QString("Ch%1").arg(startIndex + i)));
        outputs = { Port::makeNumber("Composite Out") };
        name = QString("Comp. Write Bool (%1ch @%2)").arg(channelCount).arg(startIndex);
    }
};
