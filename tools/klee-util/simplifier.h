#pragma once

#include "klee/Expr.h"

namespace kutil {

bool simplify_extract(klee::ref<klee::Expr> extract_expr,
                      klee::ref<klee::Expr> &out);

}