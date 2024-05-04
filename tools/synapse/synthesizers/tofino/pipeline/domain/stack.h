#pragma once

#include <string>
#include <vector>

#include "klee/Expr.h"

#include "variable.h"

namespace synapse {
namespace synthesizer {
namespace tofino {

struct stack_t {
  std::vector<std::vector<Variable>> stack;

  stack_t() { push(); }

  void push() { stack.emplace_back(); }

  void pop() {
    assert(stack.size());
    stack.pop_back();
  }

  std::vector<Variable> get() const {
    std::vector<Variable> vars;

    for (auto frame : stack) {
      vars.insert(vars.end(), frame.begin(), frame.end());
    }

    return vars;
  }

  variable_query_t get_by_label(const std::string& label) const {
    for (const auto &frame : stack) {
      for (const auto &variable : frame) {
        if (variable.get_label() == label) {
          return variable_query_t(variable, 0);
        }
      }
    }

    return variable_query_t();
  }

  variable_query_t get(std::string symbol) const {
    for (const auto &frame : stack) {
      for (const auto &variable : frame) {
        if (variable.match(symbol)) {
          return variable_query_t(variable, 0);
        }
      }
    }

    return variable_query_t();
  }

  variable_query_t get(klee::ref<klee::Expr> expr) const {
    for (const auto &frame : stack) {
      for (const auto &variable : frame) {
        if (variable.match(expr)) {
          return variable_query_t(variable, 0);
        }

        auto contains_result = variable.contains(expr);

        if (contains_result.contains) {
          return variable_query_t(variable, contains_result.offset_bits);
        }
      }
    }

    return variable_query_t();
  }

  void append(Variable var) {
    assert(stack.size());
    stack.back().push_back(var);
  }
};

} // namespace tofino
} // namespace synthesizer
} // namespace synapse