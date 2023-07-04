#include "exprs.h"
#include "printer.h"
#include "solver_toolbox.h"

#include <assert.h>
#include <unordered_map>

namespace kutil {

bool simplify_extract(klee::ref<klee::Expr> extract_expr,
                      klee::ref<klee::Expr> &out) {
  if (extract_expr->getKind() != klee::Expr::Extract) {
    return false;
  }

  auto extract = static_cast<klee::ExtractExpr *>(extract_expr.get());

  auto offset = extract->offset;
  auto expr = extract->expr;
  auto size = extract->width;

  // concats
  while (expr->getKind() == klee::Expr::Kind::Concat) {
    auto lhs = expr->getKid(0);
    auto rhs = expr->getKid(1);

    auto rhs_size = rhs->getWidth();

    // Either completely on the right side, or completely on the left side
    assert(
        rhs_size >= offset + size ||
        (offset >= rhs_size && lhs->getWidth() >= (offset - rhs_size) + size));

    if (rhs_size >= offset + size) {
      expr = rhs;
    } else {
      offset -= rhs_size;
      expr = lhs;
    }
  }

  assert(!expr.isNull());
  assert(size <= expr->getWidth());

  if (offset == 0 && size == expr->getWidth()) {
    out = expr;
    return true;
  }

  auto new_extract = solver_toolbox.exprBuilder->Extract(expr, offset, size);

  out = new_extract;
  return true;
}

bool is_cmp_0(klee::ref<klee::Expr> expr,
              klee::ref<klee::Expr> &not_const_kid) {
  assert(!expr.isNull());

  if (expr->getNumKids() != 2) {
    return false;
  }

  auto lhs = expr->getKid(0);
  auto rhs = expr->getKid(1);

  auto lhs_is_constant = kutil::is_constant(lhs);
  auto rhs_is_constant = kutil::is_constant(rhs);

  if (!lhs_is_constant && !rhs_is_constant) {
    return false;
  }

  auto constant = lhs_is_constant ? lhs : rhs;
  auto other = lhs_is_constant ? rhs : lhs;

  auto allowed_exprs = std::vector<klee::Expr::Kind>{
      klee::Expr::Or,  klee::Expr::And, klee::Expr::Eq,  klee::Expr::Ne,
      klee::Expr::Ult, klee::Expr::Ule, klee::Expr::Ugt, klee::Expr::Uge,
      klee::Expr::Slt, klee::Expr::Sle, klee::Expr::Sgt, klee::Expr::Sge,
  };

  auto found_it =
      std::find(allowed_exprs.begin(), allowed_exprs.end(), other->getKind());

  if (found_it == allowed_exprs.end()) {
    return false;
  }

  auto constant_value = kutil::solver_toolbox.value_from_expr(constant);
  not_const_kid = other;

  return constant_value == 0;
}

// E.g.
// (Extract 0
//   (ZExt w8
//      (Eq (w32 0) (ReadLSB w32 (w32 0) out_of_space__64))))
bool is_extract_0_cond(klee::ref<klee::Expr> expr,
                       klee::ref<klee::Expr> &cond_expr) {
  assert(!expr.isNull());

  if (expr->getKind() != klee::Expr::Extract) {
    return false;
  }

  auto extract = static_cast<klee::ExtractExpr *>(expr.get());

  if (extract->offset != 0) {
    return false;
  }

  auto kid = extract->expr;

  if (kid->getKind() == klee::Expr::ZExt) {
    kid = kid->getKid(0);
  }

  assert(kid->getKind() != klee::Expr::ZExt);

  auto cond_ops = std::vector<klee::Expr::Kind>{
      klee::Expr::Or,  klee::Expr::And, klee::Expr::Eq,  klee::Expr::Ne,
      klee::Expr::Ult, klee::Expr::Ule, klee::Expr::Ugt, klee::Expr::Uge,
      klee::Expr::Slt, klee::Expr::Sle, klee::Expr::Sgt, klee::Expr::Sge,
  };

  auto is_cond = std::find(cond_ops.begin(), cond_ops.end(), kid->getKind()) !=
                 cond_ops.end();

  if (is_cond) {
    cond_expr = kid;
    return true;
  }

  return false;
}

bool can_be_negated(klee::ref<klee::Expr> expr) {
  auto allowed = std::unordered_set<klee::Expr::Kind>{
      {klee::Expr::Eq},  {klee::Expr::Ne},  {klee::Expr::Ult},
      {klee::Expr::Uge}, {klee::Expr::Ule}, {klee::Expr::Ugt},
      {klee::Expr::Slt}, {klee::Expr::Sge}, {klee::Expr::Sle},
      {klee::Expr::Sgt}, {klee::Expr::Or},  {klee::Expr::And},
  };

  if (allowed.find(expr->getKind()) != allowed.end()) {
    return true;
  }

  auto transparent = std::unordered_set<klee::Expr::Kind>{
      {klee::Expr::ZExt},
      {klee::Expr::SExt},
  };

  if (transparent.find(expr->getKind()) != transparent.end()) {
    assert(expr->getNumKids() == 1);
    auto kid = expr->getKid(0);
    return can_be_negated(kid);
  }

  return false;
}

klee::ref<klee::Expr> negate(klee::ref<klee::Expr> expr) {
  klee::ref<klee::Expr> other;

  if (is_cmp_0(expr, other) && kutil::is_bool(other)) {
    return other;
  }

  auto negate_map = std::unordered_map<klee::Expr::Kind, klee::Expr::Kind>{
      {klee::Expr::Or, klee::Expr::And},
      {klee::Expr::And, klee::Expr::Or},
      {klee::Expr::Eq, klee::Expr::Ne},
      {klee::Expr::Ne, klee::Expr::Eq},
      {klee::Expr::Ult, klee::Expr::Uge},
      {klee::Expr::Uge, klee::Expr::Ult},
      {klee::Expr::Ule, klee::Expr::Ugt},
      {klee::Expr::Ugt, klee::Expr::Ule},
      {klee::Expr::Slt, klee::Expr::Sge},
      {klee::Expr::Sge, klee::Expr::Slt},
      {klee::Expr::Sle, klee::Expr::Sgt},
      {klee::Expr::Sgt, klee::Expr::Sle},
      {klee::Expr::ZExt, klee::Expr::ZExt},
      {klee::Expr::SExt, klee::Expr::SExt},
  };

  auto kind = expr->getKind();
  auto found_it = negate_map.find(kind);
  assert(found_it != negate_map.end());

  switch (found_it->second) {
  case klee::Expr::Or: {
    auto lhs = expr->getKid(0);
    auto rhs = expr->getKid(1);

    if (can_be_negated(lhs) && can_be_negated(rhs)) {
      lhs = negate(lhs);
      rhs = negate(rhs);
      return kutil::solver_toolbox.exprBuilder->Or(lhs, rhs);
    }

    return kutil::solver_toolbox.exprBuilder->Not(expr);
  } break;
  case klee::Expr::And: {
    auto lhs = expr->getKid(0);
    auto rhs = expr->getKid(1);

    if (can_be_negated(lhs) && can_be_negated(rhs)) {
      lhs = negate(lhs);
      rhs = negate(rhs);
      return kutil::solver_toolbox.exprBuilder->And(lhs, rhs);
    }

    return kutil::solver_toolbox.exprBuilder->Not(expr);
  } break;
  case klee::Expr::Eq: {
    auto lhs = expr->getKid(0);
    auto rhs = expr->getKid(1);
    return kutil::solver_toolbox.exprBuilder->Eq(lhs, rhs);
  } break;
  case klee::Expr::Ne: {
    auto lhs = expr->getKid(0);
    auto rhs = expr->getKid(1);
    return kutil::solver_toolbox.exprBuilder->Ne(lhs, rhs);
  } break;
  case klee::Expr::Ult: {
    auto lhs = expr->getKid(0);
    auto rhs = expr->getKid(1);
    return kutil::solver_toolbox.exprBuilder->Ult(lhs, rhs);
  } break;
  case klee::Expr::Ule: {
    auto lhs = expr->getKid(0);
    auto rhs = expr->getKid(1);
    return kutil::solver_toolbox.exprBuilder->Ule(lhs, rhs);
  } break;
  case klee::Expr::Ugt: {
    auto lhs = expr->getKid(0);
    auto rhs = expr->getKid(1);
    return kutil::solver_toolbox.exprBuilder->Ugt(lhs, rhs);
  } break;
  case klee::Expr::Uge: {
    auto lhs = expr->getKid(0);
    auto rhs = expr->getKid(1);
    return kutil::solver_toolbox.exprBuilder->Uge(lhs, rhs);
  } break;
  case klee::Expr::Slt: {
    auto lhs = expr->getKid(0);
    auto rhs = expr->getKid(1);
    return kutil::solver_toolbox.exprBuilder->Slt(lhs, rhs);
  } break;
  case klee::Expr::Sle: {
    auto lhs = expr->getKid(0);
    auto rhs = expr->getKid(1);
    return kutil::solver_toolbox.exprBuilder->Sle(lhs, rhs);
  } break;
  case klee::Expr::Sgt: {
    auto lhs = expr->getKid(0);
    auto rhs = expr->getKid(1);
    return kutil::solver_toolbox.exprBuilder->Sgt(lhs, rhs);
  } break;
  case klee::Expr::Sge: {
    auto lhs = expr->getKid(0);
    auto rhs = expr->getKid(1);
    return kutil::solver_toolbox.exprBuilder->Sge(lhs, rhs);
  } break;
  case klee::Expr::ZExt: {
    auto kid = expr->getKid(0);
    auto width = expr->getWidth();

    if (can_be_negated(kid)) {
      kid = negate(kid);
      return kutil::solver_toolbox.exprBuilder->ZExt(kid, width);
    }

    return kutil::solver_toolbox.exprBuilder->Not(expr);
  } break;
  case klee::Expr::SExt: {
    auto kid = expr->getKid(0);
    auto width = expr->getWidth();

    if (can_be_negated(kid)) {
      kid = negate(kid);
      return kutil::solver_toolbox.exprBuilder->SExt(kid, width);
    }

    return kutil::solver_toolbox.exprBuilder->Not(expr);
  } break;
  default:
    assert(false && "TODO");
  }

  return expr;
}

// Simplify expressions like (Ne (0) (And X Y)) which could be (And X Y)
bool simplify_cmp_ne_0(klee::ref<klee::Expr> expr, klee::ref<klee::Expr> &out) {
  klee::ref<klee::Expr> other;

  if (expr->getKind() != klee::Expr::Ne) {
    return false;
  }

  if (!is_cmp_0(expr, other)) {
    return false;
  }

  if (!kutil::is_bool(other)) {
    return false;
  }

  out = negate(other);
  return true;
}

// Simplify expressions like (Eq 0 (And X Y)) which could be (Or !X !Y),
// or (Eq 0 (Eq A B)) => (Ne A B)
bool simplify_cmp_eq_0(klee::ref<klee::Expr> expr, klee::ref<klee::Expr> &out) {
  klee::ref<klee::Expr> other;

  if (expr->getKind() != klee::Expr::Eq) {
    return false;
  }

  if (!is_cmp_0(expr, other)) {
    return false;
  }

  if (!kutil::is_bool(other)) {
    return false;
  }

  out = negate(other);
  return true;
}

// (Not (Eq X Y)) => (Ne X Y)
bool simplify_not_eq(klee::ref<klee::Expr> expr, klee::ref<klee::Expr> &out) {
  if (expr->getKind() != klee::Expr::Not) {
    return false;
  }

  assert(expr->getNumKids() == 1);
  expr = expr->getKid(0);

  if (expr->getKind() != klee::Expr::Eq) {
    return false;
  }

  assert(expr->getNumKids() == 2);

  auto lhs = expr->getKid(0);
  auto rhs = expr->getKid(1);

  out = kutil::solver_toolbox.exprBuilder->Ne(lhs, rhs);
  return true;
}

bool simplify_cmp_zext_eq_size(klee::ref<klee::Expr> expr,
                               klee::ref<klee::Expr> &out) {
  auto cmp_ops = std::vector<klee::Expr::Kind>{
      klee::Expr::Or,  klee::Expr::And, klee::Expr::Eq,  klee::Expr::Ne,
      klee::Expr::Ult, klee::Expr::Ule, klee::Expr::Ugt, klee::Expr::Uge,
      klee::Expr::Slt, klee::Expr::Sle, klee::Expr::Sgt, klee::Expr::Sge,
  };

  auto found_it = std::find(cmp_ops.begin(), cmp_ops.end(), expr->getKind());

  if (found_it == cmp_ops.end()) {
    return false;
  }

  assert(expr->getNumKids() == 2);

  auto lhs = expr->getKid(0);
  auto rhs = expr->getKid(1);

  auto detected = false;

  while (lhs->getKind() == klee::Expr::ZExt) {
    detected = true;
    lhs = lhs->getKid(0);
  }

  while (rhs->getKind() == klee::Expr::ZExt) {
    detected = true;
    rhs = rhs->getKid(0);
  }

  if (!detected) {
    return false;
  }

  if (lhs->getWidth() != rhs->getWidth()) {
    return false;
  }

  klee::ref<klee::Expr> kids[2] = {lhs, rhs};
  out = expr->rebuild(kids);

  return true;
}

typedef bool (*simplifier_op)(klee::ref<klee::Expr> expr,
                              klee::ref<klee::Expr> &out);

typedef std::vector<simplifier_op> simplifiers_t;

bool apply_simplifiers(const simplifiers_t &simplifiers,
                       klee::ref<klee::Expr> expr,
                       klee::ref<klee::Expr> &simplified_expr) {
  simplified_expr = expr;

  for (auto op : simplifiers) {
    if (op(expr, simplified_expr)) {
      return true;
    }
  }

  auto num_kids = expr->getNumKids();
  auto new_kids = std::vector<klee::ref<klee::Expr>>(num_kids);
  auto simplified = false;
  klee::Expr::Width max_kid_width = 0;

  for (auto i = 0u; i < num_kids; i++) {
    auto kid = expr->getKid(i);
    auto simplified_kid = kid;

    simplified |= apply_simplifiers(simplifiers, kid, simplified_kid);
    max_kid_width = std::max(max_kid_width, simplified_kid->getWidth());
    new_kids[i] = simplified_kid;
  }

  if (!simplified) {
    return false;
  }

  for (auto i = 0u; i < num_kids; i++) {
    auto simplified_kid = new_kids[i];

    if (simplified_kid->getWidth() < max_kid_width) {
      simplified_kid = kutil::solver_toolbox.exprBuilder->ZExt(simplified_kid,
                                                               max_kid_width);
    }

    assert(simplified_kid->getWidth() == max_kid_width);
    new_kids[i] = simplified_kid;
  }

  simplified_expr = expr->rebuild(&new_kids[0]);
  return true;
}

klee::ref<klee::Expr> simplify(klee::ref<klee::Expr> expr) {
  assert(!expr.isNull());

  if (expr->getKind() == klee::Expr::Constant) {
    return expr;
  }

  auto simplifiers = simplifiers_t{
      simplify_extract, simplify_cmp_eq_0, simplify_cmp_zext_eq_size,
      simplify_not_eq,  simplify_cmp_ne_0,
  };

  std::vector<klee::ref<klee::Expr>> prev_exprs{expr};

  while (true) {
    auto simplified = expr;

    apply_simplifiers(simplifiers, expr, simplified);
    assert(!simplified.isNull());

    for (auto prev : prev_exprs) {
      if (!simplified->compare(*prev.get())) {
        return expr;
      }
    }

    prev_exprs.push_back(simplified);
    expr = simplified;
  }

  return expr;
}

} // namespace kutil