#pragma once

#include "klee/Expr.h"
#include "klee/ExprBuilder.h"
#include "klee/Constraints.h"
#include "expr/Parser.h"
#include "klee/util/ExprVisitor.h"
#include "klee/util/Ref.h"
#include "retrieve_symbols.h"
#include "solver_toolbox.h"
#include <cstdint>
#include <sys/types.h>

namespace kutil {

class ConcretizeSymbols : public klee::ExprVisitor::ExprVisitor {
private:
  std::string target_symbol;
  unsigned replacement;

public:
  ConcretizeSymbols(std::string target_symbol, unsigned replacement)
      : ExprVisitor(true), target_symbol(target_symbol),
        replacement(replacement) {}

  klee::ExprVisitor::Action visitExprPost(const klee::Expr &e) {
    return Action::doChildren();
  }

  klee::ExprVisitor::Action visitRead(const klee::ReadExpr &e) {
    RetrieveSymbols retriever;

    auto expr = e;
    const klee::ref<klee::Expr> ref = &expr;
    retriever.visit(ref);

    if (retriever.get_retrieved_strings().find(target_symbol) !=
        retriever.get_retrieved_strings().end()) {
      assert(retriever.get_retrieved().size() == 1);
      klee::ref<klee::Expr> symbol = retriever.get_retrieved().front();
      assert(symbol->getNumKids() == 1);
      assert(symbol->getKid(0)->getKind() == klee::Expr::Constant);
      auto constant = symbol->getKid(0);
      uint32_t val =
          static_cast<klee::ConstantExpr *>(constant.get())->getZExtValue();

      auto new_symbol = solver_toolbox.create_new_symbol("DEVICE", 32);
      auto extract = solver_toolbox.exprBuilder->Extract(new_symbol, val,
                                                         symbol->getWidth());

      auto eq = solver_toolbox.exprBuilder->Eq(
          new_symbol, solver_toolbox.exprBuilder->Constant(
                          replacement, new_symbol->getWidth()));
      klee::ConstraintManager cm;
      cm.addConstraint(eq);

      auto replace =
          solver_toolbox.exprBuilder->Constant(replacement, symbol->getWidth());
      return Action::changeTo(replace);
    }

    return Action::doChildren();
  }
};

} // namespace kutil