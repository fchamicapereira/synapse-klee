#pragma once

#include <optional>

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

std::ostream &operator<<(std::ostream &os,
                         const ReorderingCandidateStatus &status);

struct candidate_info_t {
  node_id_t id;
  bool is_branch;
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

struct reorder_op_t {
  anchor_info_t anchor_info;
  node_id_t evicted_id;
  candidate_info_t candidate_info;
};

struct reordered_bdd_t {
  BDD *bdd;
  reorder_op_t op;

  // When the anchor is a branch, this field may contain the second reordering
  // operation that was applied to the branch.
  std::optional<reorder_op_t> op2;
};

std::vector<reordered_bdd_t> reorder(const BDD *bdd, node_id_t anchor_id,
                                     bool allow_shape_altering_ops = true);
std::vector<reordered_bdd_t> reorder(const BDD *bdd,
                                     const anchor_info_t &anchor_info,
                                     bool allow_shape_altering_ops = true);
reordered_bdd_t try_reorder(const BDD *bdd, const anchor_info_t &anchor_info,
                            node_id_t candidate_id);
std::vector<reorder_op_t> get_reorder_ops(const BDD *bdd,
                                          const anchor_info_t &anchor_info,
                                          bool allow_shape_altering_ops = true);
BDD *reorder(const BDD *bdd, const reorder_op_t &op);

double estimate_reorder(const BDD *bdd);

} // namespace bdd
