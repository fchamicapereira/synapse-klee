#pragma once

#include <string>
#include <vector>

#include "../../../code_builder.h"
#include "klee-util.h"
#include "klee/Expr.h"

namespace synapse {
namespace synthesizer {
namespace tofino {

class Variable {
protected:
  bool active;
  std::string label;
  bits_t size_bits;

  std::string prefix;
  std::vector<std::string> vigor_symbols;

  std::vector<klee::ref<klee::Expr>> exprs;

public:
  // Empty contructor for optional variables
  Variable() : active(false) {}

  Variable(const Variable &variable)
      : active(variable.active), label(variable.label),
        size_bits(variable.size_bits), prefix(variable.prefix),
        vigor_symbols(variable.vigor_symbols), exprs(variable.exprs) {}

  Variable(std::string _label, bits_t _size_bits,
           std::vector<std::string> _vigor_symbols)
      : active(true), label(_label), size_bits(_size_bits),
        vigor_symbols(_vigor_symbols) {}

  Variable(std::string _label, bits_t _size_bits, klee::ref<klee::Expr> expr)
      : active(true), label(_label), size_bits(_size_bits) {
    exprs.push_back(expr);
  }

  Variable(std::string _label, bits_t _size_bits)
      : active(true), label(_label), size_bits(_size_bits) {}

  bool is_active() const { return active; }

  void set_prefix(const std::string &_prefix) { prefix = _prefix; }

  std::string get_label() const {
    if (prefix.size()) {
      return prefix + "." + label;
    }

    return label;
  }

  bits_t get_size_bits() const { return size_bits; }

  void add_expr(klee::ref<klee::Expr> expr) { exprs.push_back(expr); }

  void add_exprs(const std::vector<klee::ref<klee::Expr>> &_exprs) {
    exprs.insert(exprs.end(), _exprs.begin(), _exprs.end());
  }

  void add_symbols(const std::vector<std::string> &symbols) {
    vigor_symbols.insert(vigor_symbols.end(), symbols.begin(), symbols.end());
  }

  void set_expr(klee::ref<klee::Expr> expr) {
    assert(exprs.size() == 0);
    return exprs.push_back(expr);
  }

  const std::vector<klee::ref<klee::Expr>> &get_exprs() const { return exprs; }

  klee::ref<klee::Expr> get_expr() const {
    assert(exprs.size() == 1);
    return exprs[0];
  }

  virtual std::string get_type() const {
    std::stringstream type;

    if (size_bits == 1) {
      type << "bool";
    } else {
      type << "bit<";
      type << size_bits;
      type << ">";
    }

    return type.str();
  }

  bool match(const std::string &s) const {
    for (const auto &symbol : vigor_symbols) {
      if (symbol == s) {
        return true;
      }
    }

    return false;
  }

  bool match(klee::ref<klee::Expr> e) const {
    for (auto expr : exprs) {
      auto eq = kutil::solver_toolbox.are_exprs_always_equal(expr, e);

      if (eq) {
        return true;
      }
    }

    return false;
  }

  kutil::solver_toolbox_t::contains_result_t
  contains(klee::ref<klee::Expr> e) const {
    for (auto expr : exprs) {
      auto contains_result = kutil::solver_toolbox.contains(expr, e);

      if (contains_result.contains) {
        return contains_result;
      }
    }

    return kutil::solver_toolbox_t::contains_result_t();
  }

  void synthesize(CodeBuilder &builder) const {
    auto type = get_type();

    builder.indent();
    builder.append(type);
    builder.append(" ");
    builder.append(label);
    builder.append(";");
    builder.append_new_line();
  }

  bool operator==(const Variable &other) const {
    return label == other.get_label();
  }
};

struct variable_query_t {
  bool valid;
  std::unique_ptr<Variable> var;
  unsigned offset_bits;

  variable_query_t() : valid(false) {}

  variable_query_t(const Variable &_var, unsigned _offset_bits)
      : valid(true), var(std::unique_ptr<Variable>(new Variable(_var))),
        offset_bits(_offset_bits) {}
};

typedef std::vector<Variable> Variables;

} // namespace tofino
} // namespace synthesizer
} // namespace synapse