#include "expr/Parser.h"
#include "klee/Constraints.h"
#include "klee/ExprBuilder.h"
#include "klee/util/ExprVisitor.h"

#include "exprs.h"
#include "retrieve_symbols.h"
#include "solver_toolbox.h"

namespace kutil {

class ExpressionFilter : public klee::ExprVisitor::ExprVisitor {
private:
  std::vector<std::string> allowed_symbols;

public:
  ExpressionFilter(const std::vector<std::string> &_allowed_symbols)
      : ExprVisitor(true), allowed_symbols(_allowed_symbols) {}

  struct expr_info_t {
    bool has_allowed;
    bool has_not_allowed;
  };

  expr_info_t check_symbols(klee::ref<klee::Expr> expr) const {
    expr_info_t info{false, false};

    SymbolRetriever retriever;
    retriever.visit(expr);

    auto symbols = retriever.get_retrieved_strings();

    for (auto s : symbols) {
      auto found_it =
          std::find(allowed_symbols.begin(), allowed_symbols.end(), s);

      if (found_it != allowed_symbols.end()) {
        info.has_allowed = true;
      } else {
        info.has_not_allowed = true;
      }
    }

    return info;
  }

  // Acts only if either lhs or rhs has **only** allowed symbols, while the
  // other has not allowed symbols. Returns the expression with allowed symbols.
  klee::ref<klee::Expr>
  pick_filtered_expression(klee::ref<klee::Expr> lhs,
                           klee::ref<klee::Expr> rhs) const {
    auto lhs_info = check_symbols(lhs);
    auto rhs_info = check_symbols(rhs);

    if (lhs_info.has_allowed && !lhs_info.has_not_allowed &&
        rhs_info.has_not_allowed) {
      return lhs;
    }

    if (rhs_info.has_allowed && !rhs_info.has_not_allowed &&
        lhs_info.has_not_allowed) {
      return rhs;
    }

    return klee::ref<klee::Expr>();
  }

#define REPLACE_BINARY_OP_WITH_ALLOWED_CHILD(T)                                \
  klee::ExprVisitor::Action visit##T(const klee::T##Expr &e) {                 \
    if (e.getNumKids() != 2)                                                   \
      return Action::doChildren();                                             \
    auto lhs = e.getKid(0);                                                    \
    auto rhs = e.getKid(1);                                                    \
    auto new_expr = pick_filtered_expression(lhs, rhs);                        \
    if (new_expr.isNull())                                                     \
      return Action::doChildren();                                             \
    return Action::changeTo(new_expr);                                         \
  }

  REPLACE_BINARY_OP_WITH_ALLOWED_CHILD(And)
  REPLACE_BINARY_OP_WITH_ALLOWED_CHILD(Or)
};

klee::ref<klee::Expr> filter(klee::ref<klee::Expr> expr,
                             const std::vector<std::string> &allowed_symbols) {
  auto filter = kutil::ExpressionFilter(allowed_symbols);

  auto data = filter.check_symbols(expr);

  if (!data.has_allowed && data.has_not_allowed) {
    return solver_toolbox.exprBuilder->True();
  }

  auto filtered = filter.visit(expr);
  assert(filter.check_symbols(filtered).has_not_allowed == 0);

  return filtered;
}

} // namespace kutil