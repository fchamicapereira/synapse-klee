#pragma once

#include "klee/ExprBuilder.h"
#include "klee/perf-contracts.h"
#include "klee/util/ArrayCache.h"
#include "klee/util/ExprSMTLIBPrinter.h"
#include "klee/util/ExprVisitor.h"

#include <ostream>

#include "../load-call-paths/load-call-paths.h"

namespace kutil {

std::string expr_to_string(klee::ref<klee::Expr> expr, bool one_liner = false);
std::string pretty_print_expr(klee::ref<klee::Expr> expr,
                              bool use_signed = true);

} // namespace kutil

std::ostream &operator<<(std::ostream &os, const klee::ref<klee::Expr> &expr);
std::ostream &operator<<(std::ostream &os, const arg_t &arg);
std::ostream &operator<<(std::ostream &os, const call_t &call);
std::ostream &operator<<(std::ostream &str, const call_path_t &cp);