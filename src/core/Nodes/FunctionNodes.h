#pragma once

#include "../Node.h"
#include <QString>
#include <QMap>
#include <cmath>
#include <stdexcept>
#include <functional>

// ─────────────────────────────────────────────────────────────────────────────
// ExpressionEvaluator
//
// A recursive-descent parser and evaluator for Stormworks function expressions.
// Supports:
//   Operators: + - * / % ^ (right-associative)
//   Unary:     - (negation)
//   Functions: sin cos tan asin acos atan atan2 max min ceil floor
//              round abs sgn sqrt len len2 lerp clamp
//   Variables: supplied as a QMap<QString,double> at eval time
// ─────────────────────────────────────────────────────────────────────────────
class ExpressionEvaluator {
public:
    // Evaluate the expression string with the given variable bindings.
    // Returns 0.0 on parse/eval error.
    double evaluate(const QString& expr, const QMap<QString, double>& vars)
    {
        src_  = expr.trimmed();
        pos_  = 0;
        vars_ = &vars;
        try {
            double result = parseExpr();
            return result;
        } catch (...) {
            return 0.0;
        }
    }

private:
    QString                   src_;
    int                       pos_ = 0;
    const QMap<QString,double>* vars_ = nullptr;

    // ── Lexer helpers ─────────────────────────────────────────────────────────
    void skipSpaces() {
        while (pos_ < src_.size() && src_[pos_].isSpace()) ++pos_;
    }

    QChar peek() {
        skipSpaces();
        return (pos_ < src_.size()) ? src_[pos_] : QChar(0);
    }

    QChar consume() {
        skipSpaces();
        return (pos_ < src_.size()) ? src_[pos_++] : QChar(0);
    }

    bool match(QChar c) {
        if (peek() == c) { consume(); return true; }
        return false;
    }

    // ── Grammar ───────────────────────────────────────────────────────────────
    // expr   → term (('+' | '-') term)*
    // term   → power (('*' | '/' | '%') power)*
    // power  → unary ('^' power)?      (right-associative)
    // unary  → '-' unary | primary
    // primary → number | variable | function '(' args ')' | '(' expr ')'

    double parseExpr() {
        double left = parseTerm();
        while (true) {
            if      (match('+')) left += parseTerm();
            else if (match('-')) left -= parseTerm();
            else break;
        }
        return left;
    }

    double parseTerm() {
        double left = parsePower();
        while (true) {
            if      (match('*')) left *= parsePower();
            else if (match('/')) { double r = parsePower(); left = (r != 0.0) ? left/r : 0.0; }
            else if (match('%')) { double r = parsePower(); left = (r != 0.0) ? std::fmod(left, r) : 0.0; }
            else break;
        }
        return left;
    }

    double parsePower() {
        double base = parseUnary();
        if (match('^')) {
            double exp = parsePower(); // right-associative
            return std::pow(base, exp);
        }
        return base;
    }

    double parseUnary() {
        if (peek() == '-') { consume(); return -parseUnary(); }
        if (peek() == '+') { consume(); return  parseUnary(); }
        return parsePrimary();
    }

    double parsePrimary() {
        skipSpaces();

        // Number literal
        if (peek().isDigit() || peek() == '.') {
            return parseNumber();
        }

        // Parenthesised subexpression
        if (peek() == '(') {
            consume(); // '('
            double val = parseExpr();
            match(')');
            return val;
        }

        // Identifier: variable or function call
        if (peek().isLetter() || peek() == '_') {
            QString name = parseIdent();

            // Function call
            if (peek() == '(') {
                consume(); // '('
                // Collect comma-separated arguments
                QVector<double> args;
                if (peek() != ')') {
                    args.push_back(parseExpr());
                    while (match(',')) args.push_back(parseExpr());
                }
                match(')');
                return callFunction(name, args);
            }

            // Variable lookup
            if (vars_ && vars_->contains(name))
                return (*vars_)[name];

            return 0.0; // unknown identifier
        }

        return 0.0;
    }

    double parseNumber() {
        int start = pos_;
        while (pos_ < src_.size() && (src_[pos_].isDigit() || src_[pos_] == '.')) ++pos_;
        // Optional exponent
        if (pos_ < src_.size() && (src_[pos_] == 'e' || src_[pos_] == 'E')) {
            ++pos_;
            if (pos_ < src_.size() && (src_[pos_] == '+' || src_[pos_] == '-')) ++pos_;
            while (pos_ < src_.size() && src_[pos_].isDigit()) ++pos_;
        }
        return src_.mid(start, pos_ - start).toDouble();
    }

    QString parseIdent() {
        int start = pos_;
        while (pos_ < src_.size() && (src_[pos_].isLetterOrNumber() || src_[pos_] == '_')) ++pos_;
        return src_.mid(start, pos_ - start);
    }

    // ── Function dispatch ─────────────────────────────────────────────────────
    double callFunction(const QString& name, const QVector<double>& a)
    {
        auto arg = [&](int i) -> double { return (i < a.size()) ? a[i] : 0.0; };

        if (name == "sin")   return std::sin(arg(0));
        if (name == "cos")   return std::cos(arg(0));
        if (name == "tan")   return std::tan(arg(0));
        if (name == "asin")  return std::asin(arg(0));
        if (name == "acos")  return std::acos(arg(0));
        if (name == "atan")  return std::atan(arg(0));
        if (name == "atan2") return std::atan2(arg(0), arg(1));
        if (name == "max")   return std::max(arg(0), arg(1));
        if (name == "min")   return std::min(arg(0), arg(1));
        if (name == "ceil")  return std::ceil(arg(0));
        if (name == "floor") return std::floor(arg(0));
        if (name == "round") {
            // round(x, y) — round x to y decimal places
            if (a.size() >= 2) {
                double factor = std::pow(10.0, arg(1));
                return std::round(arg(0) * factor) / factor;
            }
            return std::round(arg(0));
        }
        if (name == "abs")   return std::abs(arg(0));
        if (name == "sgn")   return (arg(0) > 0) ? 1.0 : (arg(0) < 0) ? -1.0 : 0.0;
        if (name == "sqrt")  return std::sqrt(arg(0));
        if (name == "len")   return std::sqrt(arg(0)*arg(0) + arg(1)*arg(1));
        if (name == "len2")  return arg(0)*arg(0) + arg(1)*arg(1);
        if (name == "lerp")  return arg(0) + (arg(1) - arg(0)) * arg(2);
        if (name == "clamp") return std::clamp(arg(0), arg(1), arg(2));
        return 0.0; // unknown function
    }
};

// ─────────────────────────────────────────────────────────────────────────────
// Function Node base — shared expression storage and evaluator
// ─────────────────────────────────────────────────────────────────────────────
class FunctionNodeBase : public Node {
public:
    QString expression;

protected:
    ExpressionEvaluator eval_;

    FunctionNodeBase(NodeType type, const QString& name)
        : Node(type, name) {}
};

// ─────────────────────────────────────────────────────────────────────────────
// Function (1 input) — type 45
// in[0]=x(number) → out[0]=result(number)
// Expression uses variable: x
// ─────────────────────────────────────────────────────────────────────────────
class FunctionNode1 : public FunctionNodeBase {
public:
    FunctionNode1() : FunctionNodeBase(NodeType::Function1, "Function") {
        inputs  = { Port::makeNumber("x") };
        outputs = { Port::makeNumber("Out") };
    }
    void evaluate() override {
        QMap<QString,double> vars = { {"x", inputNumber(0)} };
        outputNumber(0, eval_.evaluate(expression, vars));
    }
};

// ─────────────────────────────────────────────────────────────────────────────
// Function (3 inputs) — type 10
// in[0]=x, in[1]=y, in[2]=z (number) → out[0]=result(number)
// ─────────────────────────────────────────────────────────────────────────────
class FunctionNode3 : public FunctionNodeBase {
public:
    FunctionNode3() : FunctionNodeBase(NodeType::Function3, "Function") {
        inputs  = { Port::makeNumber("x"), Port::makeNumber("y"), Port::makeNumber("z") };
        outputs = { Port::makeNumber("Out") };
    }
    void evaluate() override {
        QMap<QString,double> vars = {
            {"x", inputNumber(0)}, {"y", inputNumber(1)}, {"z", inputNumber(2)}
        };
        outputNumber(0, eval_.evaluate(expression, vars));
    }
};

// ─────────────────────────────────────────────────────────────────────────────
// Function (8 inputs) — type 36
// in[0..7]=x,y,z,w,a,b,c,d (number) → out[0]=result(number)
// ─────────────────────────────────────────────────────────────────────────────
class FunctionNode8 : public FunctionNodeBase {
public:
    FunctionNode8() : FunctionNodeBase(NodeType::Function8, "Function") {
        inputs  = { Port::makeNumber("x"), Port::makeNumber("y"),
                    Port::makeNumber("z"), Port::makeNumber("w"),
                    Port::makeNumber("a"), Port::makeNumber("b"),
                    Port::makeNumber("c"), Port::makeNumber("d") };
        outputs = { Port::makeNumber("Out") };
    }
    void evaluate() override {
        QMap<QString,double> vars = {
            {"x", inputNumber(0)}, {"y", inputNumber(1)},
            {"z", inputNumber(2)}, {"w", inputNumber(3)},
            {"a", inputNumber(4)}, {"b", inputNumber(5)},
            {"c", inputNumber(6)}, {"d", inputNumber(7)}
        };
        outputNumber(0, eval_.evaluate(expression, vars));
    }
};

// ─────────────────────────────────────────────────────────────────────────────
// Function Boolean (4 inputs) — type 46
// in[0..3]=x,y,z,w (bool) → out[0]=result(bool)
// Variables are 1.0 for true, 0.0 for false; result > 0.5 = true
// ─────────────────────────────────────────────────────────────────────────────
class FunctionBoolNode4 : public FunctionNodeBase {
public:
    FunctionBoolNode4() : FunctionNodeBase(NodeType::FunctionBool4, "Bool Function") {
        inputs  = { Port::makeBool("x"), Port::makeBool("y"),
                    Port::makeBool("z"), Port::makeBool("w") };
        outputs = { Port::makeBool("Out") };
    }
    void evaluate() override {
        QMap<QString,double> vars = {
            {"x", inputBool(0)?1.0:0.0}, {"y", inputBool(1)?1.0:0.0},
            {"z", inputBool(2)?1.0:0.0}, {"w", inputBool(3)?1.0:0.0}
        };
        outputBool(0, eval_.evaluate(expression, vars) > 0.5);
    }
};

// ─────────────────────────────────────────────────────────────────────────────
// Function Boolean (8 inputs) — type 47
// in[0..7]=x,y,z,w,a,b,c,d (bool) → out[0]=result(bool)
// ─────────────────────────────────────────────────────────────────────────────
class FunctionBoolNode8 : public FunctionNodeBase {
public:
    FunctionBoolNode8() : FunctionNodeBase(NodeType::FunctionBool8, "Bool Function") {
        inputs  = { Port::makeBool("x"), Port::makeBool("y"),
                    Port::makeBool("z"), Port::makeBool("w"),
                    Port::makeBool("a"), Port::makeBool("b"),
                    Port::makeBool("c"), Port::makeBool("d") };
        outputs = { Port::makeBool("Out") };
    }
    void evaluate() override {
        QMap<QString,double> vars = {
            {"x", inputBool(0)?1.0:0.0}, {"y", inputBool(1)?1.0:0.0},
            {"z", inputBool(2)?1.0:0.0}, {"w", inputBool(3)?1.0:0.0},
            {"a", inputBool(4)?1.0:0.0}, {"b", inputBool(5)?1.0:0.0},
            {"c", inputBool(6)?1.0:0.0}, {"d", inputBool(7)?1.0:0.0}
        };
        outputBool(0, eval_.evaluate(expression, vars) > 0.5);
    }
};
