#pragma once

#include "call-paths-to-bdd.h"
#include "klee-util.h"

#include <assert.h>

namespace BDD {
namespace emulation {

typedef klee::ConstraintManager context_t;

inline std::ostream &operator<<(std::ostream &os, const context_t &ctx) {
  for (auto c : ctx) {
    os << kutil::expr_to_string(c) << "\n";
  }

  return os;
}

inline void concretize(context_t &ctx, klee::ref<klee::Expr> expr,
                       uint64_t value) {
  assert(expr->getWidth() <= 64);
  auto value_expr =
      kutil::solver_toolbox.exprBuilder->Constant(value, expr->getWidth());
  auto concretize_expr =
      kutil::solver_toolbox.exprBuilder->Eq(expr, value_expr);
  ctx.addConstraint(concretize_expr);
}

inline void concretize(context_t &ctx, klee::ref<klee::Expr> expr,
                       const uint8_t *value) {
  auto width = expr->getWidth();

  auto byte = value[0];
  auto value_expr = kutil::solver_toolbox.exprBuilder->Constant(byte, 8);

  for (auto b = 1u; b < width / 8; b++) {
    auto byte = value[b];
    auto byte_expr = kutil::solver_toolbox.exprBuilder->Constant(byte, 8);
    value_expr =
        kutil::solver_toolbox.exprBuilder->Concat(byte_expr, value_expr);
  }

  auto concretize_expr =
      kutil::solver_toolbox.exprBuilder->Eq(expr, value_expr);
  ctx.addConstraint(concretize_expr);
}

} // namespace emulation
} // namespace BDD