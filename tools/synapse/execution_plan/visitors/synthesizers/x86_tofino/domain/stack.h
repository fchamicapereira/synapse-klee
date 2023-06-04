#pragma once

#include <string>
#include <vector>

#include "klee/Expr.h"

#include "variable.h"

namespace synapse {
namespace synthesizer {
namespace x86_tofino {

struct stack_t {
  std::vector<std::vector<Variable>> stack;

  stack_t() { push(); }

  stack_t(const std::vector<Variable> &vars) : stack_t() {
    for (auto v : vars) {
      append(v);
    }
  }

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

  variable_query_t get(addr_t addr) const {
    for (const auto &frame : stack) {
      for (const auto &variable : frame) {
        if (variable.match(addr)) {
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

  std::string get_new_label(const std::string &base) const {
    int matches = 0;

    auto has_same_base = [&](const std::string &label) {
      auto delim = label.find(base);
      return delim != std::string::npos && delim == 0;
    };

    for (const auto &frame : stack) {
      for (const auto &variable : frame) {
        if (has_same_base(variable.get_label())) {
          matches++;
        }
      }
    }

    if (matches == 0) {
      return base;
    }

    return base + "_" + std::to_string(matches);
  }
};

} // namespace x86_tofino
} // namespace synthesizer
} // namespace synapse