#pragma once

#include "klee/ExprBuilder.h"
#include "klee/Solver.h"
#include "klee/util/ArrayCache.h"

#include "../load-call-paths/load-call-paths.h"

#include "replace_symbols.h"

namespace kutil {

struct solver_toolbox_t {
  klee::Solver *solver;
  klee::ExprBuilder *exprBuilder;
  klee::ArrayCache arr_cache;

  solver_toolbox_t() : solver(nullptr) {}

  void build() {
    if (solver != nullptr) {
      return;
    }

    solver = klee::createCoreSolver(klee::Z3_SOLVER);
    assert(solver);

    solver = createCexCachingSolver(solver);
    solver = createCachingSolver(solver);
    solver = createIndependentSolver(solver);

    exprBuilder = klee::createDefaultExprBuilder();
  }

  klee::ref<klee::Expr> create_new_symbol(const std::string &symbol_name,
                                          klee::Expr::Width width) const;

  bool is_expr_always_true(klee::ref<klee::Expr> expr) const;
  bool is_expr_always_true(klee::ConstraintManager constraints,
                           klee::ref<klee::Expr> expr) const;
  bool is_expr_always_true(klee::ConstraintManager constraints,
                           klee::ref<klee::Expr> expr,
                           ReplaceSymbols &symbol_replacer) const;

  bool is_expr_maybe_true(klee::ConstraintManager constraints,
                          klee::ref<klee::Expr> expr) const;
  bool is_expr_maybe_false(klee::ConstraintManager constraints,
                           klee::ref<klee::Expr> expr) const;

  bool is_expr_always_false(klee::ref<klee::Expr> expr) const;
  bool is_expr_always_false(klee::ConstraintManager constraints,
                            klee::ref<klee::Expr> expr) const;
  bool is_expr_always_false(klee::ConstraintManager constraints,
                            klee::ref<klee::Expr> expr,
                            ReplaceSymbols &symbol_replacer) const;

  bool are_exprs_always_equal(klee::ref<klee::Expr> e1,
                              klee::ref<klee::Expr> e2,
                              klee::ConstraintManager c1,
                              klee::ConstraintManager c2) const;

  bool are_exprs_always_not_equal(klee::ref<klee::Expr> e1,
                                  klee::ref<klee::Expr> e2,
                                  klee::ConstraintManager c1,
                                  klee::ConstraintManager c2) const;
  bool are_exprs_always_equal(klee::ref<klee::Expr> expr1,
                              klee::ref<klee::Expr> expr2) const;

  bool are_exprs_values_always_equal(klee::ref<klee::Expr> expr1,
                                     klee::ref<klee::Expr> expr2) const;

  struct contains_result_t {
    bool contains;
    unsigned offset_bits;
    klee::ref<klee::Expr> contained_expr;

    contains_result_t() : contains(false) {}

    contains_result_t(unsigned _offset_bits,
                      klee::ref<klee::Expr> _contained_expr)
        : contains(true), offset_bits(_offset_bits),
          contained_expr(_contained_expr) {}
  };

  contains_result_t contains(klee::ref<klee::Expr> expr1,
                             klee::ref<klee::Expr> expr2) const;

  uint64_t value_from_expr(klee::ref<klee::Expr> expr) const;
  uint64_t value_from_expr(klee::ref<klee::Expr> expr,
                           klee::ConstraintManager constraints) const;
  int64_t signed_value_from_expr(klee::ref<klee::Expr> expr,
                                 klee::ConstraintManager constraints) const;

  bool are_calls_equal(call_t c1, call_t c2) const;
};

extern solver_toolbox_t solver_toolbox;

} // namespace kutil