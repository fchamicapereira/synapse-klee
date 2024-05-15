#pragma once

#include "call-paths-to-bdd.h"

namespace bdd {

struct candidate_info_t {
  node_id_t id;
  std::unordered_set<node_id_t> siblings;
  klee::ref<klee::Expr> condition;
};

struct anchor_info_t {
  node_id_t id;
  bool direction; // When the anchor is a branch, this field indicates the
                  // direction of the branch (true or false). The reordering
                  // operation will respect this direction.
};

// Get conditions required for reordering the BDD using the proposed anchor.
bool concretize_reordering_candidate(const BDD &bdd,
                                     const anchor_info_t &anchor_info,
                                     node_id_t proposed_candidate_id,
                                     candidate_info_t &candidate_info);

std::vector<candidate_info_t>
get_reordering_candidates(const BDD &bdd, const anchor_info_t &anchor_info);

std::vector<BDD> reorder(const BDD &bdd, const anchor_info_t &anchor_info);
BDD reorder(const BDD &bdd, const anchor_info_t &anchor_info,
            const candidate_info_t &candidate_info);

std::vector<BDD>
reorder(const BDD &bdd, const anchor_info_t &anchor_info,
        const std::unordered_set<node_id_t> &furthest_back_nodes);

std::vector<BDD> get_all_reordered_bdds(const BDD &bdd, int max_reordering);
double approximate_total_reordered_bdds(const BDD &bdd);

} // namespace bdd
