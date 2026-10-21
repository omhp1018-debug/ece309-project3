#include "tools/calculator.h"
#include <vector>
namespace {
int precedence(CalcToken::Kind kind) {
    if (kind == CalcToken::Kind::Times || kind == CalcToken::Kind::Divide) return 2;
    if (kind == CalcToken::Kind::Plus || kind == CalcToken::Kind::Minus) return 1;
    return 0;
}
bool apply(std::vector<double>& values, std::vector<CalcToken::Kind>& ops,
           std::string* error) {
    if (ops.empty() || values.size() < 2) {
        *error = "malformed expression";
        return false;
    }
    const CalcToken::Kind op = ops.back();
    ops.pop_back();
    // Pop the right operand first to preserve left op right.
    const double right = values.back();
    values.pop_back();
    const double left = values.back();
    values.pop_back();
    double result = 0.0;
    switch (op) {
        case CalcToken::Kind::Plus: result = left + right; break;
        case CalcToken::Kind::Minus: result = left - right; break;
        case CalcToken::Kind::Times: result = left * right; break;
        case CalcToken::Kind::Divide:
            if (right == 0.0) {
                *error = "division by zero";
                return false;
            }
            result = left / right;
            break;
        default:
            *error = "malformed expression";
            return false;
    }
    values.push_back(result);
    return true;
}
}
bool calc_evaluate(const std::vector<CalcToken>& tokens, double* out,
                   std::string* error) {
    if (error == nullptr || out == nullptr) return false;
    error->clear();
    if (tokens.empty()) {
        *error = "empty expression";
        return false;
    }
    std::vector<double> values;
    std::vector<CalcToken::Kind> ops;
    values.reserve(tokens.size());
    ops.reserve(tokens.size());
    for (const CalcToken& token : tokens) {
        if (token.kind == CalcToken::Kind::Number) {
            values.push_back(token.value);
        } else if (token.kind == CalcToken::Kind::LParen) {
            ops.push_back(token.kind);
        } else if (token.kind == CalcToken::Kind::RParen) {
            while (!ops.empty() && ops.back() != CalcToken::Kind::LParen) {
                if (!apply(values, ops, error)) return false;
            }
            if (ops.empty()) {
                *error = "unmatched ')'";
                return false;
            }
            ops.pop_back();
        } else {
            // Reducing equal precedence here makes +, -, *, and / left-associative.
            while (!ops.empty() && ops.back() != CalcToken::Kind::LParen &&
                   precedence(ops.back()) >= precedence(token.kind)) {
                if (!apply(values, ops, error)) return false;
            }
            ops.push_back(token.kind);
        }
    }
    while (!ops.empty()) {
        if (ops.back() == CalcToken::Kind::LParen) {
            *error = "unmatched '('";
            return false;
        }
        if (!apply(values, ops, error)) return false;
    }
    if (values.size() != 1) {
        *error = "malformed expression";
        return false;
    }
    *out = values.back();
    return true;
}
