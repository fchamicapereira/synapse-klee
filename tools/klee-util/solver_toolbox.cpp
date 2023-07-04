#include <iostream>

#include "printer.h"
#include "replace_symbols.h"
#include "retrieve_symbols.h"
#include "solver_toolbox.h"

namespace kutil {

solver_toolbox_t solver_toolbox;

klee::ref<klee::Expr>
solver_toolbox_t::create_new_symbol(const std::string &symbol_name,
                                    klee::Expr::Width width) const {
  auto domain = klee::Expr::Int32;
  auto range = klee::Expr::Int8;

  auto root = solver_toolbox.arr_cache.CreateArray(
      symbol_name, width / 8, nullptr, nullptr, domain, range);

  auto updates = klee::UpdateList(root, nullptr);
  auto read_entire_symbol = klee::ref<klee::Expr>();

  for (auto i = 0u; i < width / 8; i++) {
    auto index = exprBuilder->Constant(i, domain);

    if (read_entire_symbol.isNull()) {
      read_entire_symbol = solver_toolbox.exprBuilder->Read(updates, index);
      continue;
    }

    read_entire_symbol = exprBuilder->Concat(
        solver_toolbox.exprBuilder->Read(updates, index), read_entire_symbol);
  }

  return read_entire_symbol;
}

bool solver_toolbox_t::is_expr_always_true(klee::ref<klee::Expr> expr) const {
  klee::ConstraintManager no_constraints;
  return is_expr_always_true(no_constraints, expr);
}

bool solver_toolbox_t::is_expr_always_true(klee::ConstraintManager constraints,
                                           klee::ref<klee::Expr> expr) const {
  klee::Query sat_query(constraints, expr);

  bool result;
  bool success = solver->mustBeTrue(sat_query, result);
  assert(success);

  return result;
}

bool solver_toolbox_t::is_expr_always_true(klee::ConstraintManager constraints,
                                           klee::ref<klee::Expr> expr,
                                           bool force_symbol_merge) const {
  if (!force_symbol_merge) {
    return is_expr_always_true(constraints, expr);
  }

  auto new_constraints = merge_symbols(constraints, expr);
  return is_expr_always_true(new_constraints, expr);
}

klee::ConstraintManager
solver_toolbox_t::merge_symbols(const klee::ConstraintManager &constraints,
                                klee::ref<klee::Expr> expr) const {
  if (expr.isNull()) {
    return constraints;
  }

  RetrieveSymbols retriever;
  retriever.visit(expr);
  auto symbols = retriever.get_retrieved();

  ReplaceSymbols replacer(symbols);

  klee::ConstraintManager renamed_constraints;

  for (auto c : constraints) {
    auto renamed_constraint = replacer.visit(c);
    renamed_constraints.addConstraint(renamed_constraint);
  }

  return renamed_constraints;
}

klee::ref<klee::Expr>
solver_toolbox_t::merge_symbols(klee::ref<klee::Expr> expr1,
                                klee::ref<klee::Expr> expr2) const {
  if (expr2.isNull() || expr1.isNull()) {
    return expr2;
  }

  RetrieveSymbols retriever;
  retriever.visit(expr1);
  auto updates = retriever.get_retrieved_roots_updates();

  ReplaceSymbols replacer(updates);
  return replacer.visit(expr2);
}

bool solver_toolbox_t::is_expr_maybe_true(klee::ConstraintManager constraints,
                                          klee::ref<klee::Expr> expr) const {
  klee::Query sat_query(constraints, expr);

  bool result;
  bool success = solver->mayBeTrue(sat_query, result);
  assert(success);

  return result;
}

bool solver_toolbox_t::is_expr_maybe_true(klee::ConstraintManager constraints,
                                          klee::ref<klee::Expr> expr,
                                          bool force_symbol_merge) const {
  if (!force_symbol_merge) {
    return is_expr_maybe_true(constraints, expr);
  }

  auto new_constraints = merge_symbols(constraints, expr);
  return is_expr_maybe_true(new_constraints, expr);
}

bool solver_toolbox_t::is_expr_maybe_false(klee::ConstraintManager constraints,
                                           klee::ref<klee::Expr> expr) const {
  klee::Query sat_query(constraints, expr);

  bool result;
  bool success = solver->mayBeFalse(sat_query, result);
  assert(success);

  return result;
}

bool solver_toolbox_t::is_expr_maybe_false(klee::ConstraintManager constraints,
                                          klee::ref<klee::Expr> expr,
                                          bool force_symbol_merge) const {
  if (!force_symbol_merge) {
    return is_expr_maybe_false(constraints, expr);
  }

  auto new_constraints = merge_symbols(constraints, expr);
  return is_expr_maybe_false(new_constraints, expr);
}

bool solver_toolbox_t::are_exprs_always_equal(
    klee::ref<klee::Expr> e1, klee::ref<klee::Expr> e2,
    klee::ConstraintManager c1, klee::ConstraintManager c2) const {
  auto eq_expr = exprBuilder->Eq(e1, e2);

  auto eq_in_e1_ctx_sat_query = klee::Query(c1, eq_expr);
  auto eq_in_e2_ctx_sat_query = klee::Query(c2, eq_expr);

  bool eq_in_e1_ctx;
  bool eq_in_e2_ctx;

  bool eq_in_e1_ctx_success =
      solver->mustBeTrue(eq_in_e1_ctx_sat_query, eq_in_e1_ctx);
  bool eq_in_e2_ctx_success =
      solver->mustBeTrue(eq_in_e2_ctx_sat_query, eq_in_e2_ctx);

  assert(eq_in_e1_ctx_success);
  assert(eq_in_e2_ctx_success);

  return eq_in_e1_ctx && eq_in_e2_ctx;
}

bool solver_toolbox_t::are_exprs_always_not_equal(
    klee::ref<klee::Expr> e1, klee::ref<klee::Expr> e2,
    klee::ConstraintManager c1, klee::ConstraintManager c2) const {
  auto eq_expr = exprBuilder->Eq(e1, e2);

  auto eq_in_e1_ctx_sat_query = klee::Query(c1, eq_expr);
  auto eq_in_e2_ctx_sat_query = klee::Query(c2, eq_expr);

  bool not_eq_in_e1_ctx;
  bool not_eq_in_e2_ctx;

  bool not_eq_in_e1_ctx_success =
      solver->mustBeFalse(eq_in_e1_ctx_sat_query, not_eq_in_e1_ctx);
  bool not_eq_in_e2_ctx_success =
      solver->mustBeFalse(eq_in_e2_ctx_sat_query, not_eq_in_e2_ctx);

  assert(not_eq_in_e1_ctx_success);
  assert(not_eq_in_e2_ctx_success);

  return not_eq_in_e1_ctx && not_eq_in_e2_ctx;
}

bool solver_toolbox_t::is_expr_always_false(klee::ref<klee::Expr> expr) const {
  klee::ConstraintManager no_constraints;
  return is_expr_always_false(no_constraints, expr);
}

bool solver_toolbox_t::is_expr_always_false(klee::ConstraintManager constraints,
                                            klee::ref<klee::Expr> expr) const {
  klee::Query sat_query(constraints, expr);

  bool result;
  bool success = solver->mustBeFalse(sat_query, result);
  assert(success);

  return result;
}

bool solver_toolbox_t::is_expr_always_false(klee::ConstraintManager constraints,
                                            klee::ref<klee::Expr> expr,
                                            bool force_symbol_merge) const {
  if (!force_symbol_merge) {
    return is_expr_always_false(constraints, expr);
  }

  auto new_constraints = merge_symbols(constraints, expr);
  return is_expr_always_false(new_constraints, expr);
}

bool solver_toolbox_t::are_exprs_always_equal(
    klee::ref<klee::Expr> expr1, klee::ref<klee::Expr> expr2) const {
  if (expr1.isNull() != expr2.isNull()) {
    return false;
  }

  if (expr1.isNull()) {
    return true;
  }

  if (expr1->getWidth() != expr2->getWidth()) {
    return false;
  }

  auto eq = exprBuilder->Eq(expr1, expr2);
  return is_expr_always_true(eq);
}

bool solver_toolbox_t::are_exprs_always_equal(klee::ref<klee::Expr> expr1,
                                              klee::ref<klee::Expr> expr2,
                                              bool force_symbol_merge) const {
  if (!force_symbol_merge) {
    return are_exprs_always_equal(expr1, expr2);
  }

  auto new_expr_2 = merge_symbols(expr1, expr2);
  return are_exprs_always_equal(expr1, new_expr_2);
}

bool solver_toolbox_t::are_exprs_values_always_equal(
    klee::ref<klee::Expr> expr1, klee::ref<klee::Expr> expr2) const {
  if (expr1.isNull() != expr2.isNull()) {
    return false;
  }

  if (expr1.isNull()) {
    return true;
  }

  auto v1 = value_from_expr(expr1);
  auto v2 = value_from_expr(expr2);

  auto v1_const = exprBuilder->Constant(v1, expr1->getWidth());
  auto v2_const = exprBuilder->Constant(v2, expr2->getWidth());

  auto always_v1 = are_exprs_always_equal(v1_const, expr1);
  auto always_v2 = are_exprs_always_equal(v2_const, expr2);

  if (!always_v1) {
    std::cerr << "are_exprs_values_always_equal error\n";
    std::cerr << "expr1 not always = " << expr_to_string(v1_const) << "\n";
    std::cerr << "expr1: " << expr_to_string(expr1) << "\n";
    assert(false && "are_exprs_values_always_equal error");
    exit(1);
  }

  if (!always_v2) {
    std::cerr << "are_exprs_values_always_equal error\n";
    std::cerr << "expr2 not always = " << expr_to_string(v2_const) << "\n";
    std::cerr << "expr2: " << expr_to_string(expr2) << "\n";
    assert(false && "are_exprs_values_always_equal error");
    exit(1);
  }

  return v1 == v2;
}

uint64_t solver_toolbox_t::value_from_expr(klee::ref<klee::Expr> expr) const {
  klee::ConstraintManager no_constraints;
  klee::Query sat_query(no_constraints, expr);

  klee::ref<klee::ConstantExpr> value_expr;
  bool success = solver->getValue(sat_query, value_expr);

  assert(success);
  return value_expr->getZExtValue();
}

uint64_t
solver_toolbox_t::value_from_expr(klee::ref<klee::Expr> expr,
                                  klee::ConstraintManager constraints) const {
  klee::Query sat_query(constraints, expr);

  klee::ref<klee::ConstantExpr> value_expr;
  bool success = solver->getValue(sat_query, value_expr);

  assert(success);
  return value_expr->getZExtValue();
}

bool solver_toolbox_t::are_calls_equal(call_t c1, call_t c2) const {
  if (c1.function_name != c2.function_name) {
    return false;
  }

  for (auto arg : c1.args) {
    auto found = c2.args.find(arg.first);
    if (found == c2.args.end()) {
      return false;
    }

    auto arg1 = arg.second;
    auto arg2 = found->second;

    auto expr1 = arg1.expr;
    auto expr2 = arg2.expr;

    auto in1 = arg1.in;
    auto in2 = arg2.in;

    auto out1 = arg1.out;
    auto out2 = arg2.out;

    if (expr1.isNull() != expr2.isNull()) {
      return false;
    }

    if (in1.isNull() != in2.isNull()) {
      return false;
    }

    if (out1.isNull() != out2.isNull()) {
      return false;
    }

    if (in1.isNull() && out1.isNull() &&
        !are_exprs_always_equal(expr1, expr2)) {
      return false;
    }

    if (!in1.isNull() && !are_exprs_always_equal(in1, in2)) {
      return false;
    }
  }

  return true;
}

solver_toolbox_t::contains_result_t
solver_toolbox_t::contains(klee::ref<klee::Expr> expr1,
                           klee::ref<klee::Expr> expr2) const {
  auto expr1_size_bits = expr1->getWidth();
  auto expr2_size_bits = expr2->getWidth();

  if (expr1_size_bits < expr2_size_bits) {
    return contains_result_t();
  }

  for (auto offset_bits = 0u; offset_bits + expr2_size_bits <= expr1_size_bits;
       offset_bits += 8) {
    auto expr1_extracted = kutil::solver_toolbox.exprBuilder->Extract(
        expr1, offset_bits, expr2_size_bits);
    assert(expr1_extracted->getWidth() == expr2->getWidth());

    if (are_exprs_always_equal(expr1_extracted, expr2)) {
      return contains_result_t(offset_bits, expr1_extracted);
    }
  }

  return contains_result_t();
}

int64_t solver_toolbox_t::signed_value_from_expr(
    klee::ref<klee::Expr> expr, klee::ConstraintManager constraints) const {
  auto width = expr->getWidth();
  auto value = solver_toolbox.value_from_expr(expr, constraints);

  uint64_t mask = 0;
  for (uint64_t i = 0u; i < width; i++) {
    mask <<= 1;
    mask |= 1;
  }

  return -((~value + 1) & mask);
}

} // namespace kutil