#pragma once

#include "load-call-paths.h"

namespace BDD {

class ReplaceSymbols : public klee::ExprVisitor::ExprVisitor {
private:
  std::vector<klee::ref<klee::ReadExpr>> reads;
  std::map<klee::ref<klee::Expr>, klee::ref<klee::Expr>> replacements;

public:
  ReplaceSymbols(std::vector<klee::ref<klee::ReadExpr>> _reads)
      : ExprVisitor(true), reads(_reads) {}

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

      klee::ref<klee::Expr> replaced =
          klee::expr::ExprHandle(const_cast<klee::ReadExpr *>(&e));
      std::map<klee::ref<klee::Expr>, klee::ref<klee::Expr>>::const_iterator
      it = replacements.find(replaced);

      if (it != replacements.end()) {
        replacements.insert({ replaced, read });
      }

      return Action::changeTo(read);
    }

    return Action::doChildren();
  }
};

struct solver_toolbox_t {
  klee::Solver *solver;
  klee::ExprBuilder *exprBuilder;
  klee::ArrayCache arr_cache;

  solver_toolbox_t() : solver(nullptr) {}

  void build() {
    if (solver != nullptr) {
      return;
    }

    solver = klee::createCoreSolver(klee::Z3_SOLVER);
    assert(solver);

    solver = createCexCachingSolver(solver);
    solver = createCachingSolver(solver);
    solver = createIndependentSolver(solver);

    exprBuilder = klee::createDefaultExprBuilder();
  }

  klee::ref<klee::Expr> create_new_symbol(const std::string &symbol_name,
                                          klee::Expr::Width width) const;

  bool is_expr_always_true(klee::ref<klee::Expr> expr) const;
  bool is_expr_always_true(klee::ConstraintManager constraints,
                           klee::ref<klee::Expr> expr) const;
  bool is_expr_always_true(klee::ConstraintManager constraints,
                           klee::ref<klee::Expr> expr,
                           ReplaceSymbols &symbol_replacer) const;

  bool is_expr_always_false(klee::ref<klee::Expr> expr) const;
  bool is_expr_always_false(klee::ConstraintManager constraints,
                            klee::ref<klee::Expr> expr) const;
  bool is_expr_always_false(klee::ConstraintManager constraints,
                            klee::ref<klee::Expr> expr,
                            ReplaceSymbols &symbol_replacer) const;

  bool are_exprs_always_equal(klee::ref<klee::Expr> e1,
                              klee::ref<klee::Expr> e2,
                              klee::ConstraintManager c1,
                              klee::ConstraintManager c2) const;

  bool are_exprs_always_not_equal(klee::ref<klee::Expr> e1,
                                  klee::ref<klee::Expr> e2,
                                  klee::ConstraintManager c1,
                                  klee::ConstraintManager c2) const;
  bool are_exprs_always_equal(klee::ref<klee::Expr> expr1,
                              klee::ref<klee::Expr> expr2) const;

  uint64_t value_from_expr(klee::ref<klee::Expr> expr) const;
  uint64_t value_from_expr(klee::ref<klee::Expr> expr,
                           klee::ConstraintManager constraints) const;

  bool are_calls_equal(call_t c1, call_t c2) const;
};

extern solver_toolbox_t solver_toolbox;

class SwapPacketEndianness : public klee::ExprVisitor::ExprVisitor {
private:
  std::map<klee::ref<klee::Expr>, klee::ref<klee::Expr>> replacements;

public:
  SwapPacketEndianness() : klee::ExprVisitor::ExprVisitor(true) {}

  void clear_replacements() {
    replacements.clear();
  }

  bool has_swapped() const { return replacements.size(); }

  klee::ref<klee::Expr> swap(klee::ref<klee::Expr> expr) {
    if (expr.isNull()) {
      return expr;
    }

    std::cerr << "[swap] original " << expr_to_string(expr) << "\n";

    clear_replacements();

    return visit(expr);
  }

  klee::ExprVisitor::Action visitExprPost(const klee::Expr &e) {
    auto eref = klee::ref<klee::Expr>(const_cast<klee::Expr *>(&e));

    if (has_swapped()) {
      std::cerr << "[visitPost] finding " << expr_to_string(eref) << "...\n";
    }

    auto it = replacements.find(eref);

    if (it != replacements.end()) {
      if (has_swapped()) {
        std::cerr << "[visitPost] changing to " << expr_to_string(it->second) << "\n";
      }
      return Action::changeTo(it->second);
    }
    
    return Action::doChildren();
  }

  klee::ExprVisitor::Action visitRead(const klee::ReadExpr &e) {
    auto ul = e.updates;
    auto index = e.index;
    auto root = ul.root;
    auto symbol = root->getName();

    if (symbol != "packet_chunks") {
      return Action::doChildren();
    }

    auto replaced = klee::expr::ExprHandle(const_cast<klee::ReadExpr *>(&e));
    auto it = replacements.find(replaced);

    if (it != replacements.end()) {
      return Action::doChildren();
    }

    assert(index->getKind() == klee::Expr::Constant);
    auto index_const = static_cast<klee::ConstantExpr*>(index.get());
    
    auto index_value = index_const->getZExtValue();
    auto new_index_value = index_value;

    switch (index_value) {
      case 0: case 1: case 2:
      case 3: case 4: case 5:
        new_index_value = 5 - index_value;
        break;
      case 6: case 7: case 8:
      case 9: case 10: case 11:
        new_index_value = 17 - index_value;
        break;
      default:
        break;
    }

    if (new_index_value != index_value) {
      auto new_root = solver_toolbox.arr_cache.CreateArray(
            symbol, root->getSize(),
            root->constantValues.begin().base(),
            root->constantValues.end().base(), root->getDomain(),
            root->getRange());

      auto new_ul = klee::UpdateList(new_root, ul.head);
      auto new_index = solver_toolbox.exprBuilder->Constant(new_index_value, index->getWidth());
      auto replacement = solver_toolbox.exprBuilder->Read(new_ul, new_index);

      std::cerr << "[visit] " << expr_to_string(replaced) << " => " << expr_to_string(replacement) << "\n";

      replacements.insert({ replaced, replacement });

      return Action::changeTo(replacement);
    }

    return Action::doChildren();
  }
};

class RenameSymbols : public klee::ExprVisitor::ExprVisitor {
private:
  std::map<std::string, std::string> translations;
  std::map<klee::ref<klee::Expr>, klee::ref<klee::Expr>> replacements;

public:
  RenameSymbols() {}
  RenameSymbols(const RenameSymbols &renamer)
      : klee::ExprVisitor::ExprVisitor(true),
        translations(renamer.translations), replacements(renamer.replacements) {
  }

  const std::map<std::string, std::string> &get_translations() const {
    return translations;
  }

  void add_translation(std::string before, std::string after) {
    translations[before] = after;
  }

  void remove_translation(std::string before) { translations.erase(before); }

  bool has_translation(std::string before) const {
    auto found_it = translations.find(before);
    return found_it != translations.end();
  }

  void clear_replacements() { replacements.clear(); }

  klee::ref<klee::Expr> rename(klee::ref<klee::Expr> expr) {
    if (expr.isNull()) {
      return expr;
    }

    clear_replacements();

    return visit(expr);
  }

  std::vector<klee::ConstraintManager>
  rename(const std::vector<klee::ConstraintManager> &constraints_list) {
    std::vector<klee::ConstraintManager> renamed_constraints_list;

    for (auto constraints : constraints_list) {
      klee::ConstraintManager renamed_constraints;

      for (auto constraint : constraints) {
        clear_replacements();

        auto renamed_constraint = rename(constraint);
        renamed_constraints.addConstraint(renamed_constraint);
      }

      renamed_constraints_list.push_back(renamed_constraints);
    }

    return renamed_constraints_list;
  }

  klee::ExprVisitor::Action visitExprPost(const klee::Expr &e) {
    auto eref = klee::ref<klee::Expr>(const_cast<klee::Expr *>(&e));
    auto it =
        replacements.find(klee::ref<klee::Expr>(const_cast<klee::Expr *>(&e)));

    if (it != replacements.end()) {
      return Action::changeTo(it->second);
    } else {
      return Action::doChildren();
    }
  }

  klee::ExprVisitor::Action visitRead(const klee::ReadExpr &e) {
    auto ul = e.updates;
    auto root = ul.root;
    auto symbol = root->getName();

    auto found_it = translations.find(symbol);

    if (found_it != translations.end()) {
      auto replaced = klee::expr::ExprHandle(const_cast<klee::ReadExpr *>(&e));
      auto it = replacements.find(replaced);

      if (it == replacements.end()) {
        auto new_root = solver_toolbox.arr_cache.CreateArray(
            found_it->second, root->getSize(),
            root->constantValues.begin().base(),
            root->constantValues.end().base(), root->getDomain(),
            root->getRange());

        auto new_ul = klee::UpdateList(new_root, ul.head);
        auto replacement = solver_toolbox.exprBuilder->Read(new_ul, e.index);

        replacements.insert({ replaced, replacement });

        return Action::changeTo(replacement);
      }
    }

    return Action::doChildren();
  }
};

} // namespace BDD
