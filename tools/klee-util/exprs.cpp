#include "exprs.h"
#include "printer.h"
#include "retrieve_symbols.h"
#include "solver_toolbox.h"
#include "simplifier.h"

namespace kutil {

std::vector<byte_read_t> get_bytes_read(klee::ref<klee::Expr> expr) {
  std::vector<byte_read_t> bytes;

  switch (expr->getKind()) {
  case klee::Expr::Kind::Read: {
    klee::ReadExpr *read = dyn_cast<klee::ReadExpr>(expr);
    klee::ref<klee::Expr> index = read->index;

    if (index->getKind() == klee::Expr::Kind::Constant) {
      klee::ConstantExpr *index_const =
          static_cast<klee::ConstantExpr *>(index.get());

      unsigned byte = index_const->getZExtValue();
      std::string symbol = read->updates.root->name;

      bytes.push_back({byte, symbol});
    }
  } break;
  case klee::Expr::Kind::Concat: {
    klee::ConcatExpr *concat = dyn_cast<klee::ConcatExpr>(expr);

    klee::ref<klee::Expr> left = concat->getLeft();
    klee::ref<klee::Expr> right = concat->getRight();

    std::vector<byte_read_t> lbytes = get_bytes_read(left);
    std::vector<byte_read_t> rbytes = get_bytes_read(right);

    bytes.insert(bytes.end(), lbytes.begin(), lbytes.end());
    bytes.insert(bytes.end(), rbytes.begin(), rbytes.end());
  } break;
  default:
    // Nothing to do here.
    break;
  }

  return bytes;
}

klee::ref<klee::Expr> concat_lsb(klee::ref<klee::Expr> expr,
                                 klee::ref<klee::Expr> byte) {
  if (expr->getKind() != klee::Expr::Concat) {
    return solver_toolbox.exprBuilder->Concat(expr, byte);
  }

  auto lhs = expr->getKid(0);
  auto rhs = expr->getKid(1);

  klee::ref<klee::Expr> new_kids[] = {lhs, concat_lsb(rhs, byte)};
  return expr->rebuild(new_kids);
}

std::vector<expr_group_t> get_expr_groups(klee::ref<klee::Expr> expr) {
  std::vector<expr_group_t> groups;

  auto old = expr;

  auto process_read = [&](klee::ref<klee::Expr> read_expr) {
    assert(read_expr->getKind() == klee::Expr::Read);
    klee::ReadExpr *read = dyn_cast<klee::ReadExpr>(read_expr);

    klee::ref<klee::Expr> index = read->index;
    const std::string symbol = read->updates.root->name;

    assert(index->getKind() == klee::Expr::Kind::Constant);

    klee::ConstantExpr *index_const =
        static_cast<klee::ConstantExpr *>(index.get());

    unsigned byte = index_const->getZExtValue();

    if (groups.size() && groups.back().has_symbol &&
        groups.back().symbol == symbol && groups.back().offset - 1 == byte) {
      groups.back().n_bytes++;
      groups.back().offset = byte;
      groups.back().expr = concat_lsb(groups.back().expr, read_expr);
    } else {
      groups.emplace_back(expr_group_t{true, symbol, byte,
                                       read_expr->getWidth() / 8, read_expr});
    }
  };

  auto process_not_read = [&](klee::ref<klee::Expr> not_read_expr) {
    assert(not_read_expr->getKind() != klee::Expr::Read);
    unsigned size = not_read_expr->getWidth();
    assert(size % 8 == 0);
    groups.emplace_back(expr_group_t{false, "", 0, size / 8, not_read_expr});
  };

  if (expr->getKind() == klee::Expr::Extract) {
    klee::ref<klee::Expr> extract_expr = expr;
    klee::ref<klee::Expr> out;
    if (simplify_extract(extract_expr, out)) {
      expr = out;
    }
  }

  while (expr->getKind() == klee::Expr::Concat) {
    klee::ref<klee::Expr> lhs = expr->getKid(0);
    klee::ref<klee::Expr> rhs = expr->getKid(1);

    assert(lhs->getKind() != klee::Expr::Concat);

    if (lhs->getKind() == klee::Expr::Read) {
      process_read(lhs);
    } else {
      process_not_read(lhs);
    }

    expr = rhs;
  }

  if (expr->getKind() == klee::Expr::Read) {
    process_read(expr);
  } else {
    process_not_read(expr);
  }

  return groups;
}

void print_groups(const std::vector<expr_group_t> &groups) {
  std::cerr << "Groups: " << groups.size() << "\n";
  for (const auto &group : groups) {
    if (group.has_symbol) {
      std::cerr << "Group:"
                << " symbol=" << group.symbol << " offset=" << group.offset
                << " n_bytes=" << group.n_bytes
                << " expr=" << kutil::expr_to_string(group.expr, true) << "\n";
    } else {
      std::cerr << "Group: offset=" << group.offset
                << " n_bytes=" << group.n_bytes
                << " expr=" << kutil::expr_to_string(group.expr, true) << "\n";
    }
  }
}

bool is_readLSB(klee::ref<klee::Expr> expr, std::string &symbol) {
  assert(!expr.isNull());

  if (expr->getKind() != klee::Expr::Concat) {
    return false;
  }

  auto groups = get_expr_groups(expr);

  if (groups.size() <= 1) {
    return false;
  }

  const expr_group_t &group = groups[0];
  assert(group.has_symbol);
  symbol = group.symbol;

  return true;
}

bool is_packet_readLSB(klee::ref<klee::Expr> expr, bytes_t &offset,
                       int &n_bytes) {
  assert(!expr.isNull());

  if (expr->getKind() == klee::Expr::Read) {
    klee::ReadExpr *read = dyn_cast<klee::ReadExpr>(expr);
    if (read->updates.root->name != "packet_chunks") {
      return false;
    }

    klee::ref<klee::Expr> index = read->index;
    if (index->getKind() == klee::Expr::Constant) {
      klee::ConstantExpr *index_const =
          static_cast<klee::ConstantExpr *>(index.get());

      offset = index_const->getZExtValue();
      n_bytes = read->getWidth() / 8;

      return true;
    }
  }

  if (expr->getKind() != klee::Expr::Concat) {
    return false;
  }

  std::vector<expr_group_t> groups = get_expr_groups(expr);

  if (groups.size() != 1) {
    return false;
  }

  const expr_group_t &group = groups[0];
  assert(group.has_symbol);

  if (group.symbol != "packet_chunks") {
    return false;
  }

  offset = group.offset;
  n_bytes = group.n_bytes;

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
      expr->getKind() == klee::Expr::SExt ||
      expr->getKind() == klee::Expr::Not) {
    return is_bool(expr->getKid(0));
  }

  if (expr->getKind() == klee::Expr::Or || expr->getKind() == klee::Expr::And) {
    return is_bool(expr->getKid(0)) && is_bool(expr->getKid(1));
  }

  return expr->getKind() == klee::Expr::Eq ||
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
  if (expr.isNull())
    return false;

  SymbolRetriever retriever;
  retriever.visit(expr);
  return retriever.get_retrieved().size() > 0;
}

std::unordered_set<std::string> get_symbols(klee::ref<klee::Expr> expr) {
  if (expr.isNull())
    return std::unordered_set<std::string>();

  SymbolRetriever retriever;
  retriever.visit(expr);
  return retriever.get_retrieved_strings();
}

bool is_constant(klee::ref<klee::Expr> expr) {
  if (expr->getWidth() > 64)
    return false;

  if (expr->getKind() == klee::Expr::Kind::Constant)
    return true;

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

std::optional<std::string> get_symbol(klee::ref<klee::Expr> expr) {
  std::optional<std::string> symbol;

  std::unordered_set<std::string> symbols = get_symbols(expr);

  if (symbols.size() == 1) {
    symbol = *symbols.begin();
  }

  return symbol;
}

bool manager_contains(const klee::ConstraintManager &constraints,
                      klee::ref<klee::Expr> expr) {
  auto found_it = std::find_if(
      constraints.begin(), constraints.end(), [&](klee::ref<klee::Expr> e) {
        return solver_toolbox.are_exprs_always_equal(e, expr);
      });
  return found_it != constraints.end();
}

klee::ConstraintManager join_managers(const klee::ConstraintManager &m1,
                                      const klee::ConstraintManager &m2) {
  klee::ConstraintManager m;

  for (klee::ref<klee::Expr> c : m1) {
    m.addConstraint(c);
  }

  for (klee::ref<klee::Expr> c : m2) {
    if (!manager_contains(m, c))
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