#pragma once

#include "../Node.h"
#include <algorithm>
#include <cmath>

// ─────────────────────────────────────────────────────────────────────────────
// Stateful Nodes — hold internal state across simulation ticks
// ─────────────────────────────────────────────────────────────────────────────

static constexpr double kTicksPerSecond = 60.0;

// ── UP/DOWN COUNTER ───────────────────────────────────────────────────────────
// in[0]=Up(bool), in[1]=Down(bool), in[2]=Reset(bool) → out[0]=Value(number)
class UpDownCounterNode : public Node {
public:
    double step         = 1.0;
    double resetValue   = 0.0;
    double minVal       = 0.0;
    double maxVal       = 0.0;
    bool   clampEnabled = false;

    UpDownCounterNode() : Node(NodeType::UpDownCounter, "Counter") {
        inputs  = { Port::makeBool("Up"), Port::makeBool("Down"), Port::makeBool("Reset") };
        outputs = { Port::makeNumber("Value") };
    }
    void evaluate() override {
        if (inputBool(2)) {
            value_ = resetValue;
        } else {
            if (inputBool(0)) value_ += step;
            if (inputBool(1)) value_ -= step;
        }
        if (clampEnabled) {
            value_ = std::max(value_, minVal);
            value_ = std::min(value_, maxVal);
        }
        outputNumber(0, value_);
    }
    double value() const { return value_; }
    void   setValue(double v) { value_ = v; }
private:
    double value_ = 0.0;
};

// ── MEMORY REGISTER ───────────────────────────────────────────────────────────
// in[0]=Set(bool), in[1]=Reset(bool), in[2]=Value(number) → out[0]=Stored(number)
// Set takes priority over Reset. Type 13 in Stormworks XML.
class MemoryRegisterNode : public Node {
public:
    MemoryRegisterNode() : Node(NodeType::MemoryRegister, "Memory Register") {
        inputs  = { Port::makeBool("Set"), Port::makeBool("Reset"), Port::makeNumber("Value") };
        outputs = { Port::makeNumber("Stored") };
    }
    void evaluate() override {
        if (inputBool(0))      stored_ = inputNumber(2);
        else if (inputBool(1)) stored_ = 0.0;
        outputNumber(0, stored_);
    }
    double stored() const { return stored_; }
    void   setStored(double v) { stored_ = v; }
private:
    double stored_ = 0.0;
};

// ── TON — Timer On Delay ──────────────────────────────────────────────────────
// in[0]=Enable(bool), in[1]=Duration(number) → out[0]=Q(bool)
// Output goes true after Enable has been true for >= Duration. Resets when Enable false.
class TimerTONNode : public Node {
public:
    bool useTicks = false;
    TimerTONNode() : Node(NodeType::TimerTON, "TON Timer") {
        inputs  = { Port::makeBool("Enable"), Port::makeNumber("Duration") };
        outputs = { Port::makeBool("Q") };
    }
    void evaluate() override {
        bool enable = inputBool(0);
        double durationTicks = useTicks ? inputNumber(1) : inputNumber(1) * kTicksPerSecond;
        if (!enable) { elapsed_ = 0; outputBool(0, false); }
        else { elapsed_++; outputBool(0, elapsed_ >= durationTicks); }
    }
    int elapsed() const { return elapsed_; }
    void setElapsed(int e) { elapsed_ = e; }
private:
    int elapsed_ = 0;
};

// ── TOF — Timer Off Delay ─────────────────────────────────────────────────────
// in[0]=Enable(bool), in[1]=Duration(number) → out[0]=Q(bool)
// Output true while Enable is true OR elapsed < Duration after Enable went false.
class TimerTOFNode : public Node {
public:
    bool useTicks = false;
    TimerTOFNode() : Node(NodeType::TimerTOF, "TOF Timer") {
        inputs  = { Port::makeBool("Enable"), Port::makeNumber("Duration") };
        outputs = { Port::makeBool("Q") };
    }
    void evaluate() override {
        bool enable = inputBool(0);
        double durationTicks = useTicks ? inputNumber(1) : inputNumber(1) * kTicksPerSecond;
        if (enable) { elapsed_ = 0; outputBool(0, true); }
        else { elapsed_++; outputBool(0, elapsed_ < durationTicks); }
    }
    int elapsed() const { return elapsed_; }
    void setElapsed(int e) { elapsed_ = e; }
private:
    int elapsed_ = 0;
};

// ── RTO — Retentive Timer On ──────────────────────────────────────────────────
// in[0]=Enable(bool), in[1]=Duration(number), in[2]=Reset(bool) → out[0]=Q(bool)
// Accumulates elapsed time across pulses. Only resets via Reset input.
class TimerRTONode : public Node {
public:
    bool useTicks = false;
    TimerRTONode() : Node(NodeType::TimerRTO, "RTO Timer") {
        inputs  = { Port::makeBool("Enable"), Port::makeNumber("Duration"), Port::makeBool("Reset") };
        outputs = { Port::makeBool("Q") };
    }
    void evaluate() override {
        bool enable = inputBool(0);
        double durationTicks = useTicks ? inputNumber(1) : inputNumber(1) * kTicksPerSecond;
        bool reset = inputBool(2);
        if (reset) elapsed_ = 0;
        else if (enable) elapsed_++;
        outputBool(0, elapsed_ >= durationTicks && durationTicks > 0);
    }
    int elapsed() const { return elapsed_; }
    void setElapsed(int e) { elapsed_ = e; }
private:
    int elapsed_ = 0;
};

// ── RTF — Retentive Timer Off ─────────────────────────────────────────────────
// in[0]=Enable(bool), in[1]=Duration(number), in[2]=Reset(bool) → out[0]=Q(bool)
// True while elapsed < Duration. Accumulates while Enable true, resets via Reset.
class TimerRTFNode : public Node {
public:
    bool useTicks = false;
    TimerRTFNode() : Node(NodeType::TimerRTF, "RTF Timer") {
        inputs  = { Port::makeBool("Enable"), Port::makeNumber("Duration"), Port::makeBool("Reset") };
        outputs = { Port::makeBool("Q") };
    }
    void evaluate() override {
        bool enable = inputBool(0);
        double durationTicks = useTicks ? inputNumber(1) : inputNumber(1) * kTicksPerSecond;
        bool reset = inputBool(2);
        if (reset) elapsed_ = 0;
        else if (enable) elapsed_++;
        outputBool(0, elapsed_ < durationTicks);
    }
    int elapsed() const { return elapsed_; }
    void setElapsed(int e) { elapsed_ = e; }
private:
    int elapsed_ = 0;
};

// ── PID CONTROLLER ────────────────────────────────────────────────────────────
// in[0]=Setpoint(number), in[1]=PV(number), in[2]=Enable(bool) → out[0]=Output(number)
// Kp/Ki/Kd configurable via settings dialog. Resets integral on disable.
class PIDControllerNode : public Node {
public:
    double kp = 0.0;
    double ki = 0.0;
    double kd = 0.0;
    PIDControllerNode() : Node(NodeType::PIDController, "PID Controller") {
        inputs  = { Port::makeNumber("Setpoint"), Port::makeNumber("PV"), Port::makeBool("Enable") };
        outputs = { Port::makeNumber("Output") };
    }
    void evaluate() override {
        if (!inputBool(2)) { integral_ = 0.0; prevError_ = 0.0; outputNumber(0, 0.0); return; }
        double error  = inputNumber(0) - inputNumber(1);
        integral_    += error;
        double deriv  = error - prevError_;
        prevError_    = error;
        outputNumber(0, kp * error + ki * integral_ + kd * deriv);
    }
private:
    double integral_ = 0.0, prevError_ = 0.0;
};

// ── PID CONTROLLER ADVANCED ───────────────────────────────────────────────────
// in[0]=Setpoint, in[1]=PV, in[2]=Kp, in[3]=Ki, in[4]=Kd, in[5]=Enable → out[0]=Output
class PIDControllerAdvancedNode : public Node {
public:
    PIDControllerAdvancedNode() : Node(NodeType::PIDControllerAdvanced, "PID Advanced") {
        inputs  = { Port::makeNumber("Setpoint"), Port::makeNumber("PV"),
                    Port::makeNumber("Kp"), Port::makeNumber("Ki"), Port::makeNumber("Kd"),
                    Port::makeBool("Enable") };
        outputs = { Port::makeNumber("Output") };
    }
    void evaluate() override {
        if (!inputBool(5)) { integral_ = 0.0; prevError_ = 0.0; outputNumber(0, 0.0); return; }
        double error  = inputNumber(0) - inputNumber(1);
        integral_    += error;
        double deriv  = error - prevError_;
        prevError_    = error;
        outputNumber(0, inputNumber(2)*error + inputNumber(3)*integral_ + inputNumber(4)*deriv);
    }
private:
    double integral_ = 0.0, prevError_ = 0.0;
};
