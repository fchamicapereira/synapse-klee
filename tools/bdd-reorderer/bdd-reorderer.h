#pragma once

#include "call-paths-to-bdd.h"

namespace BDD {

struct reordered_bdd {
  BDD bdd;
  Node_ptr candidate;
  klee::ref<klee::Expr> condition;

  reordered_bdd(BDD _bdd, Node_ptr _candidate, klee::ref<klee::Expr> _condition)
      : bdd(_bdd), candidate(_candidate), condition(_condition) {}
};

std::vector<reordered_bdd> reorder(const BDD &bdd, Node_ptr root);

std::vector<reordered_bdd>
reorder(const BDD &bdd, Node_ptr root,
        const std::unordered_set<node_id_t> &furthest_back_nodes);

std::vector<BDD> get_all_reordered_bdds(const BDD &bdd, int max_reordering);
float approximate_number_of_reordered_bdds(const BDD &original_bdd);

} // namespace BDD
