#pragma once

#include "../Node.h"
#include <algorithm>

// ─────────────────────────────────────────────────────────────────────────────
// Stateful Nodes
//
// Nodes that hold internal state across simulation ticks. Unlike purely
// combinational nodes (which only depend on their current inputs), stateful
// nodes remember their previous state and update it each tick.
// ─────────────────────────────────────────────────────────────────────────────

// ── UP/DOWN COUNTER ───────────────────────────────────────────────────────────
// Counts up or down by a configurable step each tick, optionally clamped to
// a min/max range. A rising edge on Up increments, Down decrements, Reset
// sets the value back to 0.
//
// Ports:
//   in[0] = Up    (bool) — increment by step each tick this is true
//   in[1] = Down  (bool) — decrement by step each tick this is true
//   in[2] = Reset (bool) — set value to 0 immediately
//   out[0] = Value (number)
//
// Settings (configurable via ⚙ dialog):
//   step    — how much to add/subtract per tick (default 1, matches <i> in XML)
//   minVal  — lower clamp (0 = disabled in Stormworks, stored as 0 meaning off)
//   maxVal  — upper clamp (0 = disabled)
//   useMin  — whether minVal is active
//   useMax  — whether maxVal is active
//
// Stormworks XML: type="37", <i text="1" value="1"/>, <min text="0"/>,
//                <max text="0"/>, <r text="0"/> (reset state)
// ─────────────────────────────────────────────────────────────────────────────
class UpDownCounterNode : public Node {
public:
    double step         = 1.0;
    double resetValue   = 0.0;
    double minVal       = 0.0;
    double maxVal       = 0.0;
    bool   clampEnabled = false;

    UpDownCounterNode() : Node(NodeType::UpDownCounter, "Counter") {
        inputs  = { Port::makeBool("Up"),
                    Port::makeBool("Down"),
                    Port::makeBool("Reset") };
        outputs = { Port::makeNumber("Value") };
    }

    void evaluate() override {
        if (inputBool(2)) {
            // Reset takes priority over Up/Down
            value_ = resetValue;
        } else {
            if (inputBool(0)) value_ += step;
            if (inputBool(1)) value_ -= step;
        }

        // Apply optional min/max clamping
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

// ─────────────────────────────────────────────────────────────────────────────
// Timer Nodes
//
// All timers count elapsed ticks internally and compare against a duration.
// Duration can be set in seconds (converted to ticks using 60 tps) or ticks
// directly, controlled by the useTicks setting.
//
// Stormworks runs at 60 ticks per second.
// ─────────────────────────────────────────────────────────────────────────────

static constexpr double kTicksPerSecond = 60.0;

// ── TON — Timer On Delay ──────────────────────────────────────────────────────
// Output goes true once Enable has been continuously true for >= Duration.
// Resets (elapsed = 0, output = false) when Enable goes false.
//
// in[0] = Enable (bool)
// in[1] = Duration (number, seconds or ticks)
// out[0] = Q (bool)
class TimerTONNode : public Node {
public:
    bool useTicks = false; // false = seconds, true = ticks

    TimerTONNode() : Node(NodeType::TimerTON, "TON Timer") {
        inputs  = { Port::makeBool("Enable"), Port::makeNumber("Duration") };
        outputs = { Port::makeBool("Q") };
    }

    void evaluate() override {
        bool   enable   = inputBool(0);
        double duration = inputNumber(1);
        double durationTicks = useTicks ? duration : duration * kTicksPerSecond;

        if (!enable) {
            elapsed_ = 0;
            outputBool(0, false);
        } else {
            elapsed_++;
            outputBool(0, elapsed_ >= durationTicks);
        }
    }

    int elapsed() const { return elapsed_; }
    void setElapsed(int e) { elapsed_ = e; }

private:
    int elapsed_ = 0;
};

// ── TOF — Timer Off Delay ─────────────────────────────────────────────────────
// Output is true while Enable is true OR while elapsed < Duration after
// Enable went false. Resets when Enable goes false and duration is reached.
//
// in[0] = Enable (bool)
// in[1] = Duration (number, seconds or ticks)
// out[0] = Q (bool)
class TimerTOFNode : public Node {
public:
    bool useTicks = false;

    TimerTOFNode() : Node(NodeType::TimerTOF, "TOF Timer") {
        inputs  = { Port::makeBool("Enable"), Port::makeNumber("Duration") };
        outputs = { Port::makeBool("Q") };
    }

    void evaluate() override {
        bool   enable   = inputBool(0);
        double duration = inputNumber(1);
        double durationTicks = useTicks ? duration : duration * kTicksPerSecond;

        if (enable) {
            elapsed_ = 0;
            outputBool(0, true);
        } else {
            elapsed_++;
            outputBool(0, elapsed_ < durationTicks);
        }
    }

    int elapsed() const { return elapsed_; }
    void setElapsed(int e) { elapsed_ = e; }

private:
    int elapsed_ = 0;
};

// ── RTO — Retentive Timer On ──────────────────────────────────────────────────
// Like TON but elapsed time accumulates across multiple Enable pulses.
// Only resets when the Reset input goes true.
//
// in[0] = Enable (bool)
// in[1] = Duration (number, seconds or ticks)
// in[2] = Reset (bool)
// out[0] = Q (bool)
class TimerRTONode : public Node {
public:
    bool useTicks = false;

    TimerRTONode() : Node(NodeType::TimerRTO, "RTO Timer") {
        inputs  = { Port::makeBool("Enable"),
                    Port::makeNumber("Duration"),
                    Port::makeBool("Reset") };
        outputs = { Port::makeBool("Q") };
    }

    void evaluate() override {
        bool   enable   = inputBool(0);
        double duration = inputNumber(1);
        bool   reset    = inputBool(2);
        double durationTicks = useTicks ? duration : duration * kTicksPerSecond;

        if (reset) {
            elapsed_ = 0;
        } else if (enable) {
            elapsed_++;
        }
        // Output stays true once duration reached, until reset
        outputBool(0, elapsed_ >= durationTicks && durationTicks > 0);
    }

    int elapsed() const { return elapsed_; }
    void setElapsed(int e) { elapsed_ = e; }

private:
    int elapsed_ = 0;
};

// ── RTF — Retentive Timer Off ─────────────────────────────────────────────────
// Output is true while elapsed < Duration. Elapsed accumulates while Enable
// is true, and only resets when the Reset input goes true.
//
// in[0] = Enable (bool)
// in[1] = Duration (number, seconds or ticks)
// in[2] = Reset (bool)
// out[0] = Q (bool)
class TimerRTFNode : public Node {
public:
    bool useTicks = false;

    TimerRTFNode() : Node(NodeType::TimerRTF, "RTF Timer") {
        inputs  = { Port::makeBool("Enable"),
                    Port::makeNumber("Duration"),
                    Port::makeBool("Reset") };
        outputs = { Port::makeBool("Q") };
    }

    void evaluate() override {
        bool   enable   = inputBool(0);
        double duration = inputNumber(1);
        bool   reset    = inputBool(2);
        double durationTicks = useTicks ? duration : duration * kTicksPerSecond;

        if (reset) {
            elapsed_ = 0;
        } else if (enable) {
            elapsed_++;
        }
        outputBool(0, elapsed_ < durationTicks);
    }

    int elapsed() const { return elapsed_; }
    void setElapsed(int e) { elapsed_ = e; }

private:
    int elapsed_ = 0;
};

// ─────────────────────────────────────────────────────────────────────────────
// PID Controller Nodes
//
// Both variants accumulate integral error across ticks and store the previous
// error for the derivative term, making them stateful. When Enable is false
// the integral and derivative state resets to prevent windup on re-enable.
// ─────────────────────────────────────────────────────────────────────────────

// ── PID CONTROLLER ────────────────────────────────────────────────────────────
// Kp, Ki, Kd are set via the node's settings dialog (stored in XML as
// <kp>, <ki>, <kd> elements). Enable resets integral/derivative state
// when false to prevent windup.
//
// in[0] = Setpoint         (number)
// in[1] = Process Variable (number)
// in[2] = Enable           (bool)
// out[0] = Output          (number)
class PIDControllerNode : public Node {
public:
    double kp = 0.0;
    double ki = 0.0;
    double kd = 0.0;

    PIDControllerNode() : Node(NodeType::PIDController, "PID Controller") {
        inputs  = { Port::makeNumber("Setpoint"),
                    Port::makeNumber("Process Variable"),
                    Port::makeBool("Enable") };
        outputs = { Port::makeNumber("Output") };
    }

    void evaluate() override {
        bool enabled = inputBool(2);
        if (!enabled) {
            // Reset integral and derivative state on disable to prevent windup
            integral_  = 0.0;
            prevError_ = 0.0;
            outputNumber(0, 0.0);
            return;
        }

        double error      = inputNumber(0) - inputNumber(1);
        integral_        += error;
        double derivative = error - prevError_;
        prevError_        = error;

        outputNumber(0, kp * error + ki * integral_ + kd * derivative);
    }

private:
    double integral_  = 0.0;
    double prevError_ = 0.0;
};

// ── PID CONTROLLER ADVANCED ───────────────────────────────────────────────────
// Same as PIDController but Kp, Ki, Kd come from number input ports rather
// than fixed settings, allowing them to be tuned live during simulation.
//
// in[0] = Setpoint         (number)
// in[1] = Process Variable (number)
// in[2] = Proportional     (number)
// in[3] = Integral         (number)
// in[4] = Derivative       (number)
// in[5] = Enable           (bool)
// out[0] = Output          (number)
class PIDControllerAdvancedNode : public Node {
public:
    PIDControllerAdvancedNode() : Node(NodeType::PIDControllerAdvanced, "PID Advanced") {
        inputs  = { Port::makeNumber("Setpoint"),
                    Port::makeNumber("Process Variable"),
                    Port::makeNumber("Proportional"),
                    Port::makeNumber("Integral"),
                    Port::makeNumber("Derivative"),
                    Port::makeBool("Enable") };
        outputs = { Port::makeNumber("Output") };
    }

    void evaluate() override {
        bool enabled = inputBool(5);
        if (!enabled) {
            integral_  = 0.0;
            prevError_ = 0.0;
            outputNumber(0, 0.0);
            return;
        }

        double error      = inputNumber(0) - inputNumber(1);
        double kp         = inputNumber(2);
        double ki         = inputNumber(3);
        double kd         = inputNumber(4);
        integral_        += error;
        double derivative = error - prevError_;
        prevError_        = error;

        outputNumber(0, kp * error + ki * integral_ + kd * derivative);
    }

private:
    double integral_  = 0.0;
    double prevError_ = 0.0;
};
