#include "exprs.h"
#include "rename_symbols.h"

namespace kutil {

class SwapPacketEndianness : public klee::ExprVisitor::ExprVisitor {
public:
  SwapPacketEndianness() : klee::ExprVisitor::ExprVisitor(true) {}

  klee::ref<klee::Expr> swap_const_endianness(klee::ref<klee::Expr> expr) {
    assert(!expr.isNull());
    assert(expr->getKind() == klee::Expr::Constant);
    assert(expr->getWidth() <= klee::Expr::Int64);

    auto constant = static_cast<const klee::ConstantExpr *>(expr.get());
    auto width = constant->getWidth();
    auto value = constant->getZExtValue();
    auto new_value = 0;

    for (auto i = 0u; i < width; i += 8) {
      auto byte = (value >> i) & 0xff;
      new_value = (new_value << 8) | byte;
    }

    return solver_toolbox.exprBuilder->Constant(new_value, width);
  }

  klee::ref<klee::Expr> visit_binary_expr(klee::ref<klee::Expr> expr) {
    assert(!expr.isNull());
    assert(expr->getNumKids() == 2);

    auto lhs = expr->getKid(0);
    auto rhs = expr->getKid(1);

    auto lhs_is_pkt_read = is_packet_readLSB(lhs);
    auto rhs_is_pkt_read = is_packet_readLSB(rhs);

    // If they are either both packet reads or neither one is, then we are not
    // interested.
    if (!(lhs_is_pkt_read ^ rhs_is_pkt_read)) {
      return nullptr;
    }

    auto pkt_read = lhs_is_pkt_read ? lhs : rhs;
    auto not_pkt_read = lhs_is_pkt_read ? rhs : lhs;

    // TODO: we should consider the other types
    assert(not_pkt_read->getKind() == klee::Expr::Constant);

    auto new_constant = swap_const_endianness(not_pkt_read);

    klee::ref<klee::Expr> new_kids[2] = {pkt_read, new_constant};
    return expr->rebuild(new_kids);
  }

#define VISIT_BINARY_CMP_OP(T)                                                 \
  klee::ExprVisitor::Action visit##T(const klee::T##Expr &e) {                 \
    auto expr = const_cast<klee::T##Expr *>(&e);                               \
    auto new_expr = visit_binary_expr(expr);                                   \
    return new_expr.isNull() ? Action::doChildren()                            \
                             : Action::changeTo(new_expr);                     \
  }

  VISIT_BINARY_CMP_OP(Eq)
  VISIT_BINARY_CMP_OP(Ne)

  VISIT_BINARY_CMP_OP(Slt)
  VISIT_BINARY_CMP_OP(Sle)
  VISIT_BINARY_CMP_OP(Sgt)
  VISIT_BINARY_CMP_OP(Sge)

  VISIT_BINARY_CMP_OP(Ult)
  VISIT_BINARY_CMP_OP(Ule)
  VISIT_BINARY_CMP_OP(Ugt)
  VISIT_BINARY_CMP_OP(Uge)
};

klee::ref<klee::Expr> swap_packet_endianness(klee::ref<klee::Expr> expr) {
  kutil::SwapPacketEndianness swapper;
  return swapper.visit(expr);
}

} // namespace kutil