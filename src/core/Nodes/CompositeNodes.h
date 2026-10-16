#pragma once

#include "../Node.h"
#include <cstdint>
#include <vector>

// ─────────────────────────────────────────────────────────────────────────────
// Composite Nodes
//
// A composite wire carries a CompositeData: 32 number channels and 32 bool
// channels (see Node.h). Nodes below read, write and route those channels.
// ─────────────────────────────────────────────────────────────────────────────

// ── COMPOSITE CHANNEL INFO ────────────────────────────────────────────────────
// Describes one channel set up on a CompositeInput node, plus the value the
// I/O panel currently has for it.
struct CompositeChannel {
    int      index    = 1;                // which composite slot (1-32)
    PortType dataType = PortType::Number; // bool or number
    double   numberValue = 0.0;           // current value if dataType is Number
    bool     boolValue   = false;         // current value if dataType is Bool
};

// ── COMPOSITE INPUT (type 4 in components_bridge) ─────────────────────────────
// A composite wire coming into the MC from the vehicle. One purple output port.
// The I/O panel configures which channels it drives and their values; each tick
// the node builds a CompositeData from them.
class CompositeInputNode : public Node {
public:
    std::vector<CompositeChannel> channels;

    CompositeInputNode() : Node(NodeType::CompositeInput, "Composite Input") {
        // Default: one number channel at index 1
        channels = { { 1, PortType::Number } };
        outputs  = { Port::makeComposite("Composite") };
    }

    void setChannels(const std::vector<CompositeChannel>& ch) { channels = ch; }

    void evaluate() override {
        CompositeData data;
        for (const auto& ch : channels) {
            if (ch.dataType == PortType::Bool) data.setBool  (ch.index, ch.boolValue);
            else                               data.setNumber(ch.index, ch.numberValue);
        }
        outputComposite(0, data);
    }
};

// ── COMPOSITE OUTPUT (type 5 in components_bridge) ────────────────────────────
// A composite wire leaving the MC to the vehicle. A pure sink: one purple input
// port, no visible output. Like the other bridge nodes it is only ever edited
// in place when saving (new ones are not written to <components_bridge>).
class CompositeOutputNode : public Node {
public:
    CompositeOutputNode() : Node(NodeType::CompositeOutput, "Composite Output") {
        inputs  = { Port::makeComposite("Composite") };
        outputs = { Port::makeComposite("Value") }; // hidden; mirrors the other sink nodes
    }
    void evaluate() override { outputComposite(0, inputComposite(0)); }
};

// ── COMPOSITE PACKER ─────────────────────────────────────────────────────────
// Simulator-only helper (not a Stormworks node): number channels Ch1..ChN in,
// one composite out with those numbers on channels 1..N.
class CompositePackerNode : public Node {
public:
    int channelCount = 1;

    CompositePackerNode() : Node(NodeType::CompositePacker, "Comp. Packer") {
        setChannelCount(1);
    }

    void setChannelCount(int count) {
        channelCount = count;
        inputs.clear();
        for (int i = 0; i < count; ++i)
            inputs.push_back(Port::makeNumber(QString("Ch%1").arg(i + 1)));
        outputs = { Port::makeComposite("Composite") };
        name = QString("Comp. Packer (%1ch)").arg(count);
    }

    void evaluate() override {
        CompositeData data;
        for (int i = 0; i < (int)inputs.size(); ++i)
            data.setNumber(i + 1, inputs[i].asNumber());
        outputComposite(0, data);
    }
};

// ── COMPOSITE READ (NUMBER) ───────────────────────────────────────────────────
// Reads number channel `channelIndex` of the incoming composite.
class CompositeReadNumberNode : public Node {
public:
    int channelIndex = 1;

    CompositeReadNumberNode() : Node(NodeType::CompositeReadNumber, "Comp. Read (Num)") {
        inputs  = { Port::makeComposite("In") };
        outputs = { Port::makeNumber("Out") };
    }

    void evaluate() override {
        outputNumber(0, inputComposite(0).number(channelIndex));
    }
};

// ── COMPOSITE READ (BOOL) ─────────────────────────────────────────────────────
// Reads bool channel `channelIndex` of the incoming composite.
class CompositeReadBoolNode : public Node {
public:
    int channelIndex = 1;

    CompositeReadBoolNode() : Node(NodeType::CompositeReadBool, "Comp. Read (Bool)") {
        inputs  = { Port::makeComposite("In") };
        outputs = { Port::makeBool("Out") };
    }

    void evaluate() override {
        outputBool(0, inputComposite(0).boolean(channelIndex));
    }
};

// ── COMPOSITE SWITCHBOX ───────────────────────────────────────────────────────
// in[0]=Composite On, in[1]=Composite Off, in[2]=Switch(bool) → out[0]=Composite.
// Passes through the On composite while Switch is on, otherwise the Off one.
// Type 53 in Stormworks XML.
class CompositeSwitchboxNode : public Node {
public:
    CompositeSwitchboxNode() : Node(NodeType::CompositeSwitchbox, "Comp. Switchbox") {
        inputs  = { Port::makeComposite("Comp. On"), Port::makeComposite("Comp. Off"),
                    Port::makeBool("Switch") };
        outputs = { Port::makeComposite("Composite") };
    }
    void evaluate() override {
        outputComposite(0, inputBool(2) ? inputComposite(0) : inputComposite(1));
    }
};

// ── BINARY TO NUMBER ──────────────────────────────────────────────────────────
// in[0]=Composite → out[0]=Number. Type 55 in Stormworks XML.
// Reads the 32 bool channels as the bits of a signed 32-bit integer, channel 1
// being the least significant bit.
class BinaryToNumberNode : public Node {
public:
    BinaryToNumberNode() : Node(NodeType::BinaryToNumber, "Binary to Number") {
        inputs  = { Port::makeComposite("Composite") };
        outputs = { Port::makeNumber("Number") };
    }
    void evaluate() override {
        const CompositeData data = inputComposite(0);
        std::uint32_t bits = 0;
        for (int ch = 1; ch <= CompositeData::kChannels; ++ch)
            if (data.boolean(ch)) bits |= (std::uint32_t(1) << (ch - 1));
        outputNumber(0, double(static_cast<std::int32_t>(bits)));
    }
};

// ── NUMBER TO BINARY ──────────────────────────────────────────────────────────
// in[0]=Number → out[0]=Composite. Type 54 in Stormworks XML.
// Writes the number (as a signed 32-bit integer) onto the 32 bool channels,
// channel 1 being the least significant bit.
class NumberToBinaryNode : public Node {
public:
    NumberToBinaryNode() : Node(NodeType::NumberToBinary, "Number to Binary") {
        inputs  = { Port::makeNumber("Number") };
        outputs = { Port::makeComposite("Composite") };
    }
    void evaluate() override {
        const double v = inputNumber(0);
        // Clamp before converting so out-of-range doubles don't overflow
        const double clamped = v < -2147483648.0 ? -2147483648.0
                             : v >  2147483647.0 ?  2147483647.0 : v;
        const std::uint32_t bits = static_cast<std::uint32_t>(static_cast<std::int32_t>(clamped));
        CompositeData data;
        for (int ch = 1; ch <= CompositeData::kChannels; ++ch)
            data.setBool(ch, (bits >> (ch - 1)) & 1u);
        outputComposite(0, data);
    }
};

// ── LUA SCRIPT ────────────────────────────────────────────────────────────────
// Type 56 in Stormworks XML: composite in, composite out, and the script text in
// script="..." on <object>. The simulator keeps and saves the script but does
// not run it, so the output composite stays empty.
class LuaScriptNode : public Node {
public:
    QString script;

    LuaScriptNode() : Node(NodeType::LuaScript, "Lua Script") {
        inputs  = { Port::makeComposite("Composite") };
        outputs = { Port::makeComposite("Composite") };
    }
    void evaluate() override { outputComposite(0, CompositeData{}); }
};

// ── COMPOSITE WRITE (NUMBER) ──────────────────────────────────────────────────
// Port layout:
//   input[0]  = Composite In  (the existing composite wire to merge onto)
//   input[1+] = Channel 1, Channel 2, ... (number-typed, configurable count)
//   output[0] = Composite Out: the incoming composite with number channels
//               startIndex .. startIndex+channelCount-1 overwritten by the
//               channel inputs.
//
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
        CompositeData data = inputComposite(0);
        for (int i = 0; i < channelCount; ++i)
            data.setNumber(startIndex + i, inputNumber(1 + i));
        outputComposite(0, data);
    }

private:
    void rebuildPorts() {
        inputs.clear();
        inputs.push_back(Port::makeComposite("Composite In"));
        for (int i = 0; i < channelCount; ++i)
            inputs.push_back(Port::makeNumber(QString("Ch%1").arg(startIndex + i)));
        outputs = { Port::makeComposite("Composite Out") };
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
        CompositeData data = inputComposite(0);
        for (int i = 0; i < channelCount; ++i)
            data.setBool(startIndex + i, inputBool(1 + i));
        outputComposite(0, data);
    }

private:
    void rebuildPorts() {
        inputs.clear();
        inputs.push_back(Port::makeComposite("Composite In"));
        for (int i = 0; i < channelCount; ++i)
            inputs.push_back(Port::makeBool(QString("Ch%1").arg(startIndex + i)));
        outputs = { Port::makeComposite("Composite Out") };
        name = QString("Comp. Write Bool (%1ch @%2)").arg(channelCount).arg(startIndex);
    }
};
