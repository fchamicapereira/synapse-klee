#include "exprs.h"
#include "printer.h"
#include "retrieve_symbols.h"
#include "solver_toolbox.h"

namespace kutil {

bool get_bytes_read(klee::ref<klee::Expr> expr, std::vector<unsigned> &bytes) {
  switch (expr->getKind()) {
  case klee::Expr::Kind::Read: {
    klee::ReadExpr *read = dyn_cast<klee::ReadExpr>(expr);
    auto index = read->index;

    if (index->getKind() != klee::Expr::Kind::Constant) {
      return false;
    }

    klee::ConstantExpr *index_const =
        static_cast<klee::ConstantExpr *>(index.get());
    bytes.push_back(index_const->getZExtValue());

    return true;
  };
  case klee::Expr::Kind::Concat: {
    klee::ConcatExpr *concat = dyn_cast<klee::ConcatExpr>(expr);

    auto left = concat->getLeft();
    auto right = concat->getRight();

    if (!get_bytes_read(left, bytes) || !get_bytes_read(right, bytes)) {
      return false;
    }

    return true;
  };
  default: {
  }
  }

  return false;
}

bool is_readLSB(klee::ref<klee::Expr> expr) {
  std::string symbol;
  return is_readLSB(expr, symbol);
}

bool is_readLSB(klee::ref<klee::Expr> expr, std::string &symbol) {
  if (expr->getKind() != klee::Expr::Concat) {
    return false;
  }

  auto size = expr->getWidth();
  assert(size % 8 == 0);
  size /= 8;

  RetrieveSymbols retriever;
  retriever.visit(expr);

  auto symbols = retriever.get_retrieved_strings();

  if (symbols.size() > 1) {
    return false;
  }

  std::vector<unsigned> bytes_read;
  if (!get_bytes_read(expr, bytes_read)) {
    return false;
  }

  unsigned expected_byte = size - 1;
  for (const auto &byte : bytes_read) {
    if (byte != expected_byte) {
      return false;
    }

    expected_byte -= 1;
  }

  symbol = *symbols.begin();

  return true;
}

bool is_packet_readLSB(klee::ref<klee::Expr> expr, bytes_t &offset,
                       int &n_bytes) {
  RetrieveSymbols retriever;
  retriever.visit(expr);

  auto symbols = retriever.get_retrieved_strings();

  if (symbols.size() != 1 || *symbols.begin() != "packet_chunks") {
    return false;
  }

  std::vector<unsigned> bytes_read;
  if (!get_bytes_read(expr, bytes_read) || bytes_read.size() == 0) {
    return false;
  }

  auto expected_byte = bytes_read[0];
  for (const auto &byte : bytes_read) {
    if (byte != expected_byte) {
      return false;
    }

    expected_byte -= 1;
  }

  offset = bytes_read.back();
  n_bytes = bytes_read.size();

  return true;
}

bool is_packet_readLSB(klee::ref<klee::Expr> expr) {
  bytes_t offset;
  int n_bytes;
  return is_packet_readLSB(expr, offset, n_bytes);
}

bool is_bool(klee::ref<klee::Expr> expr) {
  assert(!expr.isNull());

  if (expr->getWidth() == 1) {
    return true;
  }

  if (expr->getKind() == klee::Expr::ZExt ||
      expr->getKind() == klee::Expr::SExt) {
    return is_bool(expr->getKid(0));
  }

  return expr->getKind() == klee::Expr::Eq ||
         expr->getKind() == klee::Expr::Not ||
         expr->getKind() == klee::Expr::Or ||
         expr->getKind() == klee::Expr::And ||
         expr->getKind() == klee::Expr::Uge ||
         expr->getKind() == klee::Expr::Ugt ||
         expr->getKind() == klee::Expr::Ule ||
         expr->getKind() == klee::Expr::Ult ||
         expr->getKind() == klee::Expr::Sge ||
         expr->getKind() == klee::Expr::Sgt ||
         expr->getKind() == klee::Expr::Sle ||
         expr->getKind() == klee::Expr::Slt;
}

bool has_symbols(klee::ref<klee::Expr> expr) {
  if (expr.isNull()) {
    return false;
  }

  RetrieveSymbols retriever(false, true);
  retriever.visit(expr);
  return retriever.get_retrieved().size() > 0;
}

std::unordered_set<std::string> get_symbols(klee::ref<klee::Expr> expr) {
  if (expr.isNull()) {
    return std::unordered_set<std::string>();
  }

  RetrieveSymbols retriever;
  retriever.visit(expr);
  return retriever.get_retrieved_strings();
}

bool is_constant(klee::ref<klee::Expr> expr) {
  if (expr->getWidth() > 64) {
    return false;
  }

  if (expr->getKind() == klee::Expr::Kind::Constant) {
    return true;
  }

  auto value = solver_toolbox.value_from_expr(expr);
  auto const_value =
      solver_toolbox.exprBuilder->Constant(value, expr->getWidth());
  auto is_always_eq = solver_toolbox.are_exprs_always_equal(const_value, expr);

  return is_always_eq;
}

bool is_constant_signed(klee::ref<klee::Expr> expr) {
  auto size = expr->getWidth();

  if (!is_constant(expr)) {
    return false;
  }

  assert(size <= 64);

  auto value = solver_toolbox.value_from_expr(expr);
  auto sign_bit = value >> (size - 1);

  return sign_bit == 1;
}

int64_t get_constant_signed(klee::ref<klee::Expr> expr) {
  if (!is_constant_signed(expr)) {
    return false;
  }

  uint64_t value;
  auto width = expr->getWidth();

  if (expr->getKind() == klee::Expr::Kind::Constant) {
    auto constant = static_cast<klee::ConstantExpr *>(expr.get());
    assert(width <= 64);
    value = constant->getZExtValue(width);
  } else {
    value = solver_toolbox.value_from_expr(expr);
  }

  uint64_t mask = 0;
  for (uint64_t i = 0u; i < width; i++) {
    mask <<= 1;
    mask |= 1;
  }

  return -((~value + 1) & mask);
}

std::pair<bool, std::string> get_symbol(klee::ref<klee::Expr> expr) {
  auto symbols = get_symbols(expr);
  auto result = std::pair<bool, std::string>();

  if (symbols.size() != 1) {
    result.first = false;
  } else {
    result.first = true;
    result.second = *symbols.begin();
  }

  return result;
}

bool manager_contains(klee::ConstraintManager constraints,
                      klee::ref<klee::Expr> expr) {
  auto found_it = std::find_if(
      constraints.begin(), constraints.end(), [&](klee::ref<klee::Expr> e) {
        return solver_toolbox.are_exprs_always_equal(e, expr, true);
      });
  return found_it != constraints.end();
}

klee::ConstraintManager join_managers(klee::ConstraintManager m1,
                                      klee::ConstraintManager m2) {
  klee::ConstraintManager m;

  for (auto c : m1) {
    m.addConstraint(c);
  }

  for (auto c : m2) {
    m.addConstraint(c);
  }

  return m;
}

addr_t expr_addr_to_obj_addr(klee::ref<klee::Expr> obj_addr) {
  assert(!obj_addr.isNull());
  assert(is_constant(obj_addr));
  return solver_toolbox.value_from_expr(obj_addr);
}

} // namespace kutil