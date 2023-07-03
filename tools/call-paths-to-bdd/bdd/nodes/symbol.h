#pragma once

#include "klee-util.h"
#include "load-call-paths.h"

namespace BDD {

typedef std::string label_t;

struct symbol_t {
  label_t label;
  label_t label_base;
  klee::ref<klee::Expr> expr;
  klee::ref<klee::Expr> addr;

  symbol_t() {}

  symbol_t(label_t _label, label_t _label_base, klee::ref<klee::Expr> _expr)
      : label(_label), label_base(_label_base), expr(_expr) {}

  symbol_t(label_t _label, label_t _label_base, klee::ref<klee::Expr> _expr,
           klee::ref<klee::Expr> _addr)
      : label(_label), label_base(_label_base), expr(_expr), addr(_addr) {}

  symbol_t(const symbol_t &symbol)
      : label(symbol.label), label_base(symbol.label_base), expr(symbol.expr),
        addr(symbol.addr) {}

  symbol_t &operator=(const symbol_t &other) {
    if (this != &other) {
      label = other.label;
      label_base = other.label_base;
      expr = other.expr;
      addr = other.addr;
    }

    return *this;
  }
};

inline bool operator==(const symbol_t &lhs, const symbol_t &rhs) {
  if (lhs.label != rhs.label) {
    return false;
  }

  if (lhs.label_base != rhs.label_base) {
    return false;
  }

  if (!kutil::solver_toolbox.are_exprs_always_equal(lhs.expr, rhs.expr, true)) {
    return false;
  }

  if (!kutil::solver_toolbox.are_exprs_always_equal(lhs.addr, rhs.addr, true)) {
    return false;
  }

  return true;
}

struct symbol_t_hash {
  std::size_t operator()(const symbol_t &_node) const {
    return std::hash<label_t>()(_node.label);
  }
};

typedef std::unordered_set<symbol_t, symbol_t_hash> symbols_t;

bool has_label(const symbols_t &symbols, const label_t &base);
label_t get_label(const symbols_t &symbols, const label_t &base);
symbol_t get_symbol(const symbols_t &symbols, const label_t &base);

} // namespace BDD