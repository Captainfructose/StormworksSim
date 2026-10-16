#pragma once

#include <QString>
#include <vector>

#include "../core/Node.h"

// ─────────────────────────────────────────────────────────────────────────────
// NodeCatalog
//
// The one list of node types a user can add, grouped by category. Both the
// "Add Node" menus (right-click and menu bar) and the Tab palette are built from
// it, so a node added here shows up everywhere.
//
// Entries of the same category must be adjacent. `separatorBefore` draws a
// divider above the entry in the menus (the palette ignores it).
// ─────────────────────────────────────────────────────────────────────────────
struct NodeCatalogEntry {
    QString  category;
    QString  label;
    NodeType type;
    bool     separatorBefore = false;
};

inline const std::vector<NodeCatalogEntry>& nodeCatalog()
{
    static const std::vector<NodeCatalogEntry> catalog = {
        // I/O
        { "I/O", "Bool Input",       NodeType::BoolInput },
        { "I/O", "Bool Output",      NodeType::BoolOutput },
        { "I/O", "Number Input",     NodeType::NumberInput },
        { "I/O", "Number Output",    NodeType::NumberOutput },
        { "I/O", "Composite Input",  NodeType::CompositeInput },
        { "I/O", "Composite Output", NodeType::CompositeOutput },
        { "I/O", "Video Input",      NodeType::VideoInput },
        { "I/O", "Video Output",     NodeType::VideoOutput },
        { "I/O", "Audio Input",      NodeType::AudioInput },
        { "I/O", "Audio Output",     NodeType::AudioOutput },

        // Constants
        { "Constants", "Constant Number", NodeType::NumberLiteral },
        { "Constants", "Constant On",     NodeType::ConstantOn },

        // Composite
        { "Composite", "Composite Packer",       NodeType::CompositePacker },
        { "Composite", "Composite Read Number",  NodeType::CompositeReadNumber },
        { "Composite", "Composite Read Bool",    NodeType::CompositeReadBool },
        { "Composite", "Composite Write Number", NodeType::CompositeWriteNumber },
        { "Composite", "Composite Write Bool",   NodeType::CompositeWriteBool },
        { "Composite", "Composite Switchbox",    NodeType::CompositeSwitchbox },
        { "Composite", "Video Switchbox",        NodeType::VideoSwitchbox },
        { "Composite", "Audio Switchbox",        NodeType::AudioSwitchbox },
        { "Composite", "Lua Script",             NodeType::LuaScript },
        { "Composite", "Binary to Number",       NodeType::BinaryToNumber },
        { "Composite", "Number to Binary",       NodeType::NumberToBinary },

        // Logic
        { "Logic", "AND",            NodeType::And },
        { "Logic", "OR",             NodeType::Or },
        { "Logic", "NOT",            NodeType::Not },
        { "Logic", "XOR",            NodeType::Xor },
        { "Logic", "NAND",           NodeType::Nand },
        { "Logic", "NOR",            NodeType::Nor },
        { "Logic", "SR Latch",       NodeType::SRLatch },
        { "Logic", "JK Flip-Flop",   NodeType::JKFlipFlop },
        { "Logic", "Capacitor",      NodeType::Capacitor },
        { "Logic", "Push to Toggle", NodeType::PushToToggle },
        { "Logic", "Toggle to Push", NodeType::ToggleToPush },

        // Math
        { "Math", "Add",          NodeType::Add },
        { "Math", "Subtract",     NodeType::Subtract },
        { "Math", "Multiply",     NodeType::Multiply },
        { "Math", "Divide",       NodeType::Divide },
        { "Math", "Clamp",        NodeType::Clamp },
        { "Math", "Abs",          NodeType::Abs },
        { "Math", "Modulo",       NodeType::Modulo },
        { "Math", "Delta",        NodeType::Delta },
        { "Math", "Threshold",    NodeType::Threshold },
        { "Math", "Switchbox",    NodeType::NumericalSwitchbox },
        { "Math", "Junction",     NodeType::NumericalJunction },
        { "Math", "Greater Than", NodeType::GreaterThan },
        { "Math", "Less Than",    NodeType::LessThan },
        { "Math", "Equal To",     NodeType::EqualTo },

        // Stateful
        { "Stateful", "Up/Down Counter", NodeType::UpDownCounter },
        { "Stateful", "Memory Register", NodeType::MemoryRegister },
        { "Stateful", "Blinker",         NodeType::Blinker },
        { "Stateful", "TON Timer",       NodeType::TimerTON,   true },
        { "Stateful", "TOF Timer",       NodeType::TimerTOF },
        { "Stateful", "RTO Timer",       NodeType::TimerRTO },
        { "Stateful", "RTF Timer",       NodeType::TimerRTF },
        { "Stateful", "PID Controller",  NodeType::PIDController, true },
        { "Stateful", "PID Advanced",    NodeType::PIDControllerAdvanced },

        // Property
        { "Property", "Dropdown",       NodeType::PropertyDropdown },
        { "Property", "Number",         NodeType::PropertyNumber },
        { "Property", "Slider",         NodeType::PropertySlider },
        { "Property", "Toggle",         NodeType::PropertyToggle },
        { "Property", "Text",           NodeType::PropertyText },
        { "Property", "Tooltip Number", NodeType::TooltipNumber, true },
        { "Property", "Tooltip On/Off", NodeType::TooltipOnOff },

        // Function
        { "Function", "Function (1 input)",       NodeType::Function1 },
        { "Function", "Function (3 inputs)",      NodeType::Function3 },
        { "Function", "Function (8 inputs)",      NodeType::Function8 },
        { "Function", "Bool Function (4 inputs)", NodeType::FunctionBool4 },
        { "Function", "Bool Function (8 inputs)", NodeType::FunctionBool8 },
    };
    return catalog;
}
