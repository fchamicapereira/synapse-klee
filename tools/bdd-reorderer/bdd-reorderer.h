#pragma once

#include "call-paths-to-bdd.h"

namespace bdd {

struct candidate_t {
  node_id_t candidate_id;
  std::unordered_set<node_id_t> siblings;
  klee::ref<klee::Expr> condition;
  klee::ref<klee::Expr> extra_condition;
};

// Get conditions required for reordering the BDD using the proposed anchor.
bool concretize_reordering_candidate(const BDD &bdd, node_id_t anchor_id,
                                     node_id_t proposed_candidate_id,
                                     candidate_t &candidate);

std::vector<BDD> reorder(const BDD &bdd, node_id_t anchor_id);
BDD reorder(const BDD &bdd, node_id_t anchor_id, const candidate_t &candidate);

std::vector<BDD>
reorder(const BDD &bdd, node_id_t anchor_id,
        const std::unordered_set<node_id_t> &furthest_back_nodes);

std::vector<BDD> get_all_reordered_bdds(const BDD &bdd, int max_reordering);
double approximate_total_reordered_bdds(const BDD &bdd);

} // namespace bdd
