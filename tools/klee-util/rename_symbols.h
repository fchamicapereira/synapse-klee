#pragma once

#include <unordered_map>

#include "expr/Parser.h"
#include "klee/Constraints.h"
#include "klee/ExprBuilder.h"
#include "klee/util/ExprVisitor.h"

#include "solver_toolbox.h"

namespace kutil {

class RenameSymbols : public klee::ExprVisitor::ExprVisitor {
private:
  std::unordered_map<std::string, std::string> translations;

public:
  RenameSymbols() {}

  RenameSymbols(const RenameSymbols &renamer)
      : klee::ExprVisitor::ExprVisitor(true),
        translations(renamer.translations) {}

  const std::unordered_map<std::string, std::string> &get_translations() const {
    return translations;
  }

  void add_translation(const std::string &before, const std::string &after) {
    translations[before] = after;
  }

  void remove_translation(const std::string &before) {
    translations.erase(before);
  }

  bool has_translation(const std::string &before) const {
    auto found_it = translations.find(before);
    return found_it != translations.end();
  }

  klee::ref<klee::Expr> rename(klee::ref<klee::Expr> expr) {
    if (expr.isNull())
      return expr;
    return visit(expr);
  }

  klee::ConstraintManager rename(const klee::ConstraintManager &constraints) {
    klee::ConstraintManager renamed_constraints;

    for (klee::ref<klee::Expr> constraint : constraints) {
      klee::ref<klee::Expr> renamed_constraint = rename(constraint);
      renamed_constraints.addConstraint(renamed_constraint);
    }

    return renamed_constraints;
  }

  call_t rename(const call_t &call) {
    call_t renamed_call = call;

    for (auto &arg_pair : renamed_call.args) {
      arg_t &arg = renamed_call.args[arg_pair.first];
      arg.expr = rename(arg.expr);
      arg.in = rename(arg.in);
      arg.out = rename(arg.out);
    }

    for (auto &extra_var_pair : renamed_call.extra_vars) {
      extra_var_t &extra_var = renamed_call.extra_vars[extra_var_pair.first];
      extra_var.first = rename(extra_var.first);
      extra_var.second = rename(extra_var.second);
    }

    renamed_call.ret = rename(renamed_call.ret);
    return renamed_call;
  }

  klee::ExprVisitor::Action visitRead(const klee::ReadExpr &e) {
    klee::UpdateList ul = e.updates;
    const klee::Array *root = ul.root;
    const std::string &symbol = root->getName();
    auto found_it = translations.find(symbol);

    if (found_it != translations.end()) {
      klee::ref<klee::Expr> replaced(const_cast<klee::ReadExpr *>(&e));
      const klee::Array *new_root = solver_toolbox.arr_cache.CreateArray(
          found_it->second, root->getSize(),
          root->constantValues.begin().base(),
          root->constantValues.end().base(), root->getDomain(),
          root->getRange());

      klee::UpdateList new_ul(new_root, ul.head);
      klee::ref<klee::Expr> replacement =
          solver_toolbox.exprBuilder->Read(new_ul, e.index);

      return Action::changeTo(replacement);
    }

    return Action::doChildren();
  }
};

} // namespace kutil