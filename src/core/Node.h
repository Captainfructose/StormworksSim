#pragma once

#include <QString>
#include <QVariant>
#include <vector>
#include <memory>

enum class PortType {
    Bool,
    Number
};

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

    bool   asBool()   const { return value.toBool(); }
    double asNumber() const { return value.toDouble(); }

    void set(bool   v) { value = v; }
    void set(double v) { value = v; }
};

enum class NodeType {
    // I/O
    BoolInput,
    BoolOutput,
    NumberInput,
    NumberOutput,
    NumberLiteral,

    // Composite
    CompositeInput,
    CompositePacker,
    CompositeReadNumber,
    CompositeReadBool,
    CompositeWriteNumber,
    CompositeWriteBool,

    // Logic
    And,
    Or,
    Not,
    Xor,
    Nand,
    Nor,

    // Math
    Add,
    Subtract,
    Multiply,
    Divide,
    Clamp,
    Abs,
    Min,
    Max,
    Modulo,
    GreaterThan,
    LessThan,
    EqualTo,
    UpDownCounter,
    TimerTON,
    TimerTOF,
    TimerRTO,
    TimerRTF,
    PIDController,
    PIDControllerAdvanced
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

    explicit Node(NodeType type, const QString& name)
        : type(type), name(name) {}

    virtual ~Node() = default;
    virtual void evaluate() = 0;

    static std::unique_ptr<Node> create(NodeType type);

    bool   inputBool  (int idx) const { return inputs[idx].asBool();   }
    double inputNumber(int idx) const { return inputs[idx].asNumber(); }

    void outputBool  (int idx, bool   v) { outputs[idx].set(v); }
    void outputNumber(int idx, double v) { outputs[idx].set(v); }
};
