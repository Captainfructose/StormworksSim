#pragma once

#include <QString>
#include <QVariant>
#include <array>
#include <vector>
#include <memory>

enum class PortType {
    Bool,
    Number,
    Composite   // a bundle of 32 number channels and 32 bool channels
};

// ── CompositeData ─────────────────────────────────────────────────────────────
// The value carried by a Composite port/wire. Like Stormworks, a composite has
// 32 number channels and 32 separate bool channels, numbered 1..32 (stored here
// at indices 0..31). Numbers and bools live in separate namespaces.
struct CompositeData {
    static constexpr int kChannels = 32;
    std::array<double, kChannels> numbers{};
    std::array<bool,   kChannels> bools{};

    // 1-based channel access; out-of-range channels read as 0/false and
    // ignore writes
    double number(int channel) const {
        return (channel >= 1 && channel <= kChannels) ? numbers[channel - 1] : 0.0;
    }
    bool boolean(int channel) const {
        return (channel >= 1 && channel <= kChannels) ? bools[channel - 1] : false;
    }
    void setNumber(int channel, double v) {
        if (channel >= 1 && channel <= kChannels) numbers[channel - 1] = v;
    }
    void setBool(int channel, bool v) {
        if (channel >= 1 && channel <= kChannels) bools[channel - 1] = v;
    }
};
Q_DECLARE_METATYPE(CompositeData)

struct Port {
    QString  label;
    PortType type;
    QVariant value;

    static Port makeBool(const QString& label, bool defaultVal = false) {
        return { label, PortType::Bool, defaultVal };
    }
    static Port makeNumber(const QString& label, double defaultVal = 0.0) {
        return { label, PortType::Number, defaultVal };
    }
    static Port makeComposite(const QString& label) {
        return { label, PortType::Composite, QVariant::fromValue(CompositeData{}) };
    }

    bool   asBool()   const { return value.toBool(); }
    double asNumber() const { return value.toDouble(); }
    // An unset port reads as an empty composite
    CompositeData asComposite() const { return value.value<CompositeData>(); }

    void set(bool   v) { value = v; }
    void set(double v) { value = v; }
    void set(const CompositeData& v) { value = QVariant::fromValue(v); }
};

enum class NodeType {
    // I/O
    BoolInput,
    BoolOutput,
    NumberInput,
    NumberOutput,
    NumberLiteral,
    PropertyDropdown,
    PropertyNumber,
    PropertySlider,
    PropertyToggle,
    PropertyText,
    TooltipNumber,
    TooltipOnOff,

    // Composite
    CompositeInput,
    CompositeOutput,
    CompositePacker,
    CompositeReadNumber,
    CompositeReadBool,
    CompositeWriteNumber,
    CompositeWriteBool,
    CompositeSwitchbox,
    BinaryToNumber,
    NumberToBinary,
    LuaScript,

    // Logic
    And,
    Or,
    Not,
    Xor,
    Nand,
    Nor,
    ConstantOn,
    PushToToggle,
    ToggleToPush,
    SRLatch,
    JKFlipFlop,
    Capacitor,

    // Math
    Add,
    Subtract,
    Multiply,
    Divide,
    Clamp,
    Abs,
    Modulo,
    Delta,
    Threshold,
    NumericalSwitchbox,

    // Comparison
    GreaterThan,
    LessThan,
    EqualTo,

    // Stateful
    UpDownCounter,
    MemoryRegister,
    Blinker,
    TimerTON,
    TimerTOF,
    TimerRTO,
    TimerRTF,
    PIDController,
    PIDControllerAdvanced,

    // Function evaluator nodes
    Function1,
    Function3,
    Function8,
    FunctionBool4,
    FunctionBool8
};

class Node {
public:
    int      id   = -1;
    QString  name;
    NodeType type;
    std::vector<Port> inputs;
    std::vector<Port> outputs;
    int posX = 0;
    int posY = 0;

    // Connector label shown in Stormworks for I/O bridge nodes (Bool/Number/
    // Composite Input/Output): the <node label="..."> in the file's <nodes>.
    // Empty means "not set" — the node's type name / a default is used.
    QString label;

    explicit Node(NodeType type, const QString& name)
        : type(type), name(name) {}

    virtual ~Node() = default;
    virtual void evaluate() = 0;

    static std::unique_ptr<Node> create(NodeType type);

    bool   inputBool  (int idx) const { return inputs[idx].asBool();   }
    double inputNumber(int idx) const { return inputs[idx].asNumber(); }

    CompositeData inputComposite(int idx) const { return inputs[idx].asComposite(); }

    void outputBool  (int idx, bool   v) { outputs[idx].set(v); }
    void outputNumber(int idx, double v) { outputs[idx].set(v); }
    void outputComposite(int idx, const CompositeData& v) { outputs[idx].set(v); }
};
