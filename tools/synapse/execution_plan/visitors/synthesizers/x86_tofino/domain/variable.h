#pragma once

#include <string>
#include <vector>

#include "../../../../memory_bank.h"
#include "../../code_builder.h"
#include "../../util.h"

#include "klee-util.h"
#include "klee/Expr.h"

namespace synapse {
namespace synthesizer {
namespace x86_tofino {

class Variable {
protected:
  std::string label;
  bits_t size_bits;
  addr_t addr;

  std::vector<std::string> vigor_symbols;
  std::vector<klee::ref<klee::Expr>> exprs;

public:
  Variable(const Variable &variable)
      : label(variable.label), size_bits(variable.size_bits), addr(variable.addr),
        vigor_symbols(variable.vigor_symbols), exprs(variable.exprs) {}

  Variable(std::string _label, bits_t _size_bits,
           std::vector<std::string> _vigor_symbols)
      : label(_label), size_bits(_size_bits), vigor_symbols(_vigor_symbols) {}

  Variable(std::string _label, bits_t _size_bits, klee::ref<klee::Expr> expr)
      : label(_label), size_bits(_size_bits) {
    exprs.push_back(expr);
  }

  Variable(std::string _label, klee::ref<klee::Expr> expr)
      : label(_label), size_bits(expr->getWidth()) {
    exprs.push_back(expr);
  }

  Variable(std::string _label, bits_t _size_bits)
      : label(_label), size_bits(_size_bits) {}

  const std::string &get_label() const { return label; }
  bits_t get_size_bits() const { return size_bits; }

  const std::vector<std::string> &get_vigor_symbols() const {
    return vigor_symbols;
  }

  void add_expr(klee::ref<klee::Expr> expr) { exprs.push_back(expr); }

  void set_expr(klee::ref<klee::Expr> expr) {
    assert(exprs.size() == 0);
    return exprs.push_back(expr);
  }

  void set_addr(addr_t _addr) { addr = _addr; }

  bool has_expr() const { return exprs.size() > 0; }

  klee::ref<klee::Expr> get_expr() const {
    assert(exprs.size() == 1);
    return exprs[0];
  }

  virtual std::string get_type() const {
    assert(size_bits > 0);
    assert(size_bits % 8 == 0);

    std::stringstream type;

    if (is_primitive_type(size_bits)) {
      type << "uint";
      type << size_bits;
      type << "_t";
      return type.str();
    } else {
      type << "bytes_t";
    }

    return type.str();
  }

  bool match(std::string s) const {
    for (const auto &symbol : vigor_symbols) {
      if (symbol == s) {
        return true;
      }

      // check if symbol is base for incoming s
      auto delim = s.find(symbol);

      if (delim == std::string::npos || delim > 0) {
        continue;
      }

      // format is "{base label}__{node id}"
      if (s.size() <= symbol.size() + 2) {
        continue;
      }

      if (s.substr(symbol.size(), 2) != "__") {
        continue;
      }

      return true;
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

  bool match(addr_t a) const { return addr == a; }

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

} // namespace x86_tofino
} // namespace synthesizer
} // namespace synapse