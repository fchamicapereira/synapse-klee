#pragma once

#include "call-paths-to-bdd.h"

namespace bdd {

enum class ReorderingCandidateStatus {
  VALID,
  UNREACHABLE_CANDIDATE,
  CANDIDATE_FOLLOWS_ANCHOR,
  IO_CHECK_FAILED,
  RW_CHECK_FAILED,
  NOT_ALLOWED,
  CONFLICTING_ROUTING,
  IMPOSSIBLE_CONDITION,
};

struct candidate_info_t {
  node_id_t id;
  std::unordered_set<node_id_t> siblings;
  klee::ref<klee::Expr> condition;
  ReorderingCandidateStatus status;
};

struct anchor_info_t {
  node_id_t id;
  bool direction; // When the anchor is a branch, this field indicates the
                  // direction of the branch (true or false). The reordering
                  // operation will respect this direction.
};
struct reordered_bdd_t {
  BDD bdd;
  anchor_info_t anchor_info;
  candidate_info_t candidate_info;
};

// Get conditions required for reordering the BDD using the proposed anchor.
candidate_info_t
concretize_reordering_candidate(const BDD &bdd,
                                const anchor_info_t &anchor_info,
                                node_id_t proposed_candidate_id);

std::vector<candidate_info_t>
get_reordering_candidates(const BDD &bdd, const anchor_info_t &anchor_info);

std::vector<reordered_bdd_t> reorder(const BDD &bdd);
std::vector<reordered_bdd_t> reorder(const BDD &bdd,
                                     const anchor_info_t &anchor_info);
BDD reorder(const BDD &bdd, const anchor_info_t &anchor_info,
            const candidate_info_t &candidate_info);

double estimate_reorder(const BDD &bdd);

} // namespace bdd
