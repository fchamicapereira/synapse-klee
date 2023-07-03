#pragma once

#include <iostream>
#include <map>
#include <unordered_map>
#include <vector>

#include "expr/Parser.h"
#include "klee/Constraints.h"
#include "klee/ExprBuilder.h"
#include "klee/util/ExprVisitor.h"

#include "solver_toolbox.h"

namespace kutil {

class ReplaceSymbols : public klee::ExprVisitor::ExprVisitor {
private:
  std::vector<klee::ref<klee::ReadExpr>> reads;
  std::unordered_map<std::string, klee::UpdateList> roots_updates;
  std::map<klee::ref<klee::Expr>, klee::ref<klee::Expr>> replacements;

public:
  ReplaceSymbols(
      const std::unordered_map<std::string, klee::UpdateList> &_roots_updates)
      : ExprVisitor(true), roots_updates(_roots_updates) {}

  ReplaceSymbols(const std::vector<klee::ref<klee::ReadExpr>> &_reads)
      : ExprVisitor(true), reads(_reads) {}

  ReplaceSymbols() : ExprVisitor(true) {}

  void add_read(klee::ref<klee::ReadExpr> read) { reads.push_back(read); }

  void add_roots_updates(
      const std::unordered_map<std::string, klee::UpdateList> &_roots_updates) {
    for (auto it = _roots_updates.begin(); it != _roots_updates.end(); it++) {
      if (roots_updates.find(it->first) != roots_updates.end()) {
        continue;
      }

      roots_updates.insert({it->first, it->second});
    }
  }

  klee::ExprVisitor::Action visitExprPost(const klee::Expr &e) {
    std::map<klee::ref<klee::Expr>, klee::ref<klee::Expr>>::const_iterator it =
        replacements.find(klee::ref<klee::Expr>(const_cast<klee::Expr *>(&e)));

    if (it != replacements.end()) {
      return Action::changeTo(it->second);
    } else {
      return Action::doChildren();
    }
  }

  klee::ExprVisitor::Action visitRead(const klee::ReadExpr &e) {
    klee::UpdateList ul = e.updates;
    const klee::Array *root = ul.root;
    auto replaced = const_cast<klee::ReadExpr *>(&e);

    if (roots_updates.find(root->name) != roots_updates.end()) {
      const auto &new_ul = roots_updates.at(root->name);
      auto new_read = solver_toolbox.exprBuilder->Read(new_ul, e.index);
      auto it = replacements.find(new_read);

      if (it == replacements.end()) {
        replacements.insert({replaced, new_read});
      }

      return Action::changeTo(new_read);
    }

    for (const auto &read : reads) {
      if (read->getWidth() != e.getWidth()) {
        continue;
      }

      if (read->index.compare(e.index) != 0) {
        continue;
      }

      if (root->name != read->updates.root->name) {
        continue;
      }

      if (root->getDomain() != read->updates.root->getDomain()) {
        continue;
      }

      if (root->getRange() != read->updates.root->getRange()) {
        continue;
      }

      if (root->getSize() != read->updates.root->getSize()) {
        continue;
      }

      auto it = replacements.find(replaced);

      if (it == replacements.end()) {
        replacements.insert({replaced, read});
      }

      return Action::changeTo(read);
    }

    return Action::doChildren();
  }
};

} // namespace kutil