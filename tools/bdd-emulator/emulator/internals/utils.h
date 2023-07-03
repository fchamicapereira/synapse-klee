#pragma once

#include "call-paths-to-bdd.h"
#include "klee-util.h"

#include "byte.h"
#include "context.h"

#include <assert.h>

namespace BDD {
namespace emulation {

inline bytes_t bytes_from_expr(klee::ref<klee::Expr> expr,
                               const context_t &ctx) {
  auto size = expr->getWidth() / 8;
  assert(expr->getWidth() % 8 == 0);

  bytes_t values(size);

  for (auto byte = 0u; byte < size; byte++) {
    auto byte_expr =
        kutil::solver_toolbox.exprBuilder->Extract(expr, byte * 8, 8);
    auto byte_value = kutil::solver_toolbox.value_from_expr(byte_expr, ctx);
    values[size - byte - 1] = byte_value;
  }

  return values;
}

} // namespace emulation
} // namespace BDD