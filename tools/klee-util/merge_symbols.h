#pragma once

#include <unordered_map>

#include "klee/util/ExprVisitor.h"

#include "retrieve_symbols.h"
#include "replace_symbols.h"
#include "solver_toolbox.h"

namespace kutil {

struct symbols_merger_t {
  std::unordered_map<std::string, klee::UpdateList> roots_updates;
  kutil::ReplaceSymbols replacer;

  klee::ref<klee::Expr> save_and_merge(klee::ref<klee::Expr> expr) {
    if (expr.isNull())
      return expr;

    kutil::SymbolRetriever retriever;
    retriever.visit(expr);

    const std::unordered_map<std::string, klee::UpdateList>
        &expr_roots_updates = retriever.get_retrieved_roots_updates();

    for (auto it = expr_roots_updates.begin(); it != expr_roots_updates.end();
         it++) {
      if (roots_updates.find(it->first) != roots_updates.end())
        continue;
      roots_updates.insert({it->first, it->second});
    }

    replacer.add_roots_updates(expr_roots_updates);
    return replacer.visit(expr);
  }

  klee::ConstraintManager
  save_and_merge(const klee::ConstraintManager &constraints) {
    klee::ConstraintManager new_constraints;

    for (klee::ref<klee::Expr> constraint : constraints) {
      klee::ref<klee::Expr> new_constraint = save_and_merge(constraint);
      new_constraints.addConstraint(new_constraint);
    }

    return new_constraints;
  }

  call_t save_and_merge(const call_t &call) {
    call_t new_call = call;

    for (auto it = call.args.begin(); it != call.args.end(); it++) {
      const arg_t &arg = call.args.at(it->first);

      new_call.args[it->first].expr = save_and_merge(arg.expr);
      new_call.args[it->first].in = save_and_merge(arg.in);
      new_call.args[it->first].out = save_and_merge(arg.out);
    }

    for (auto it = call.extra_vars.begin(); it != call.extra_vars.end(); it++) {
      const auto &extra_var = call.extra_vars.at(it->first);

      new_call.extra_vars[it->first].first = save_and_merge(extra_var.first);
      new_call.extra_vars[it->first].second = save_and_merge(extra_var.second);
    }

    new_call.ret = save_and_merge(call.ret);

    return new_call;
  }
};

} // namespace kutil
