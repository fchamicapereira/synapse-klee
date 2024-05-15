#include "bdd-reorderer.h"

// FIXME: remove this
#include "bdd-visualizer.h"

#include <iomanip>

namespace bdd {

static std::map<std::string, bool> fn_has_side_effects_lookup{
    {"rte_ether_addr_hash", false},
    {"expire_items_single_map", true},
    {"expire_items_single_map_iteratively", true},
    {"packet_borrow_next_chunk", true},
    {"packet_get_unread_length", true},
    {"packet_return_chunk", true},
    {"vector_borrow", false},
    {"vector_return", true},
    {"map_get", false},
    {"map_put", true},
    {"map_erase", true},
    {"dchain_allocate_new_index", true},
    {"dchain_is_index_allocated", false},
    {"dchain_free_index", true},
    {"dchain_rejuvenate_index", true},
    {"cht_find_preferred_available_backend", false},
    {"LoadBalancedFlow_hash", false},
    {"sketch_expire", true},
    {"sketch_compute_hashes", true},
    {"sketch_refresh", true},
    {"sketch_fetch", false},
    {"sketch_touch_buckets", true},
    {"hash_obj", false},
};

static std::vector<std::string> fn_cannot_reorder_lookup{
    "nf_set_rte_ipv4_udptcp_checksum",
    "packet_borrow_next_chunk",
    "packet_return_chunk",
};

struct mutable_vector_t {
  Node *node;
  bool direction;
};

struct vector_t {
  const Node *node;
  bool direction;
};

static Node *get_vector_next(const mutable_vector_t &vector) {
  Node *next = nullptr;

  switch (vector.node->get_type()) {
  case NodeType::BRANCH: {
    Branch *branch = static_cast<Branch *>(vector.node);
    if (vector.direction) {
      next = branch->get_mutable_on_true();
    } else {
      next = branch->get_mutable_on_false();
    }
  } break;
  case NodeType::CALL:
  case NodeType::ROUTE:
    next = vector.node->get_mutable_next();
    break;
  }

  return next;
}

static const Node *get_vector_next(const vector_t &vector) {
  const Node *next = nullptr;

  switch (vector.node->get_type()) {
  case NodeType::BRANCH: {
    const Branch *branch = static_cast<const Branch *>(vector.node);
    if (vector.direction) {
      next = branch->get_on_true();
    } else {
      next = branch->get_on_false();
    }
  } break;
  case NodeType::CALL:
  case NodeType::ROUTE:
    next = vector.node->get_next();
    break;
  }

  return next;
}

static bool fn_has_side_effects(const std::string &fn) {
  auto found = fn_has_side_effects_lookup.find(fn);
  assert(found != fn_has_side_effects_lookup.end() && "Function not found");
  return found->second;
}

static bool fn_can_be_reordered(const std::string &fn) {
  auto found_it = std::find(fn_cannot_reorder_lookup.begin(),
                            fn_cannot_reorder_lookup.end(), fn);
  return found_it == fn_cannot_reorder_lookup.end();
}

static bool read_in_chunk(klee::ref<klee::ReadExpr> read,
                          klee::ref<klee::Expr> chunk) {
  klee::ref<klee::Expr> index_expr = read->index;
  unsigned byte_read = kutil::solver_toolbox.value_from_expr(index_expr);

  std::vector<kutil::byte_read_t> known_chunk_bytes =
      kutil::get_bytes_read(chunk);

  auto is_byte_read = [byte_read](const kutil::byte_read_t &read) {
    return read.symbol == "packet_chunks" && read.offset == byte_read;
  };

  auto found_it = std::find_if(known_chunk_bytes.begin(),
                               known_chunk_bytes.end(), is_byte_read);
  return found_it != known_chunk_bytes.end();
}

static bool are_all_symbols_known(klee::ref<klee::Expr> expr,
                                  const symbols_t &known_symbols) {
  kutil::SymbolRetriever symbol_retriever;
  symbol_retriever.visit(expr);

  const std::unordered_set<std::string> &dependencies =
      symbol_retriever.get_retrieved_strings();

  if (dependencies.size() == 0) {
    return true;
  }

  bool has_packet_dependencies = false;
  for (const std::string &dependency : dependencies) {
    auto is_dependency = [dependency](const symbol_t &s) {
      return s.array->name == dependency;
    };

    auto found_it =
        std::find_if(known_symbols.begin(), known_symbols.end(), is_dependency);

    if (found_it == known_symbols.end()) {
      return false;
    }

    if (dependency == "packet_chunks") {
      has_packet_dependencies = true;
    }
  }

  if (!has_packet_dependencies) {
    return true;
  }

  const std::vector<klee::ref<klee::ReadExpr>> &packet_dependencies =
      symbol_retriever.get_retrieved_packet_chunks();

  for (klee::ref<klee::ReadExpr> dependency : packet_dependencies) {
    bool filled = false;
    for (const symbol_t &known : known_symbols) {
      if (known.array->name == "packet_chunks" &&
          read_in_chunk(dependency, known.expr)) {
        filled = true;
        break;
      }
    }

    if (!filled) {
      return false;
    }
  }

  return true;
}

static bool get_siblings(const vector_t &anchor, const Node *target,
                         bool find_in_all_branches,
                         std::unordered_set<node_id_t> &siblings) {
  const Node *anchor_next = get_vector_next(anchor);

  if (!anchor_next && find_in_all_branches) {
    return false;
  }

  std::vector<const Node *> nodes{anchor_next};
  NodeType target_type = target->get_type();

  while (nodes.size()) {
    const Node *node = nodes[0];
    nodes.erase(nodes.begin());

    if (node == target)
      continue;

    switch (node->get_type()) {
    case NodeType::BRANCH: {
      const Branch *branch_node = static_cast<const Branch *>(node);

      if (target_type == NodeType::BRANCH) {
        const Branch *target_branch = static_cast<const Branch *>(target);

        bool matching_conditions = kutil::solver_toolbox.are_exprs_always_equal(
            branch_node->get_condition(), target_branch->get_condition());

        if (matching_conditions) {
          siblings.insert(node->get_id());
          continue;
        }
      }

      const Node *on_true = branch_node->get_on_true();
      const Node *on_false = branch_node->get_on_false();

      if (on_true)
        nodes.push_back(on_true);
      if (on_false)
        nodes.push_back(on_false);
    } break;
    case NodeType::CALL: {
      const Call *call_node = static_cast<const Call *>(node);

      if (target_type == NodeType::CALL) {
        const Call *call_target = static_cast<const Call *>(target);

        bool matching_calls = kutil::solver_toolbox.are_calls_equal(
            call_node->get_call(), call_target->get_call());

        if (matching_calls) {
          siblings.insert(node->get_id());
          continue;
        }
      }

      const Node *next = call_node->get_next();

      if (next) {
        nodes.push_back(next);
      } else if (find_in_all_branches) {
        return false;
      }
    } break;
    case NodeType::ROUTE: {
      const Route *route_node = static_cast<const Route *>(node);

      if (target_type == NodeType::ROUTE) {
        const Route *route_target = static_cast<const Route *>(target);

        bool matching_route_decisions =
            route_node->get_operation() == route_target->get_operation();
        bool matching_output_port =
            route_node->get_dst_device() == route_target->get_dst_device();

        if (matching_route_decisions && matching_output_port) {
          siblings.insert(node->get_id());
          continue;
        }
      }

      const Node *next = route_node->get_next();

      if (next) {
        nodes.push_back(next);
      } else if (find_in_all_branches) {
        return false;
      }
    } break;
    }
  }

  return true;
}

static bool io_check(const Node *node, const symbols_t &anchor_symbols) {
  bool met = true;

  switch (node->get_type()) {
  case NodeType::BRANCH: {
    const Branch *branch_node = static_cast<const Branch *>(node);
    klee::ref<klee::Expr> condition = branch_node->get_condition();
    met &= are_all_symbols_known(condition, anchor_symbols);
  } break;
  case NodeType::CALL: {
    const Call *call_node = static_cast<const Call *>(node);
    const call_t &call = call_node->get_call();

    for (const std::pair<std::string, arg_t> &arg_pair : call.args) {
      const arg_t &arg = arg_pair.second;

      klee::ref<klee::Expr> expr = arg.expr;
      klee::ref<klee::Expr> in = arg.in;

      if (!expr.isNull())
        met &= are_all_symbols_known(expr, anchor_symbols);

      if (!in.isNull())
        met &= are_all_symbols_known(in, anchor_symbols);
    }
  } break;
  case NodeType::ROUTE:
    // Nothing to do here.
    break;
  }

  return met;
}

static bool io_check(klee::ref<klee::Expr> expr,
                     const symbols_t &anchor_symbols) {
  return are_all_symbols_known(expr, anchor_symbols);
}

static bool check_no_side_effects(const Node *node) {
  NodeType type = node->get_type();

  if (type != NodeType::CALL) {
    return true;
  }

  const Call *call_node = static_cast<const Call *>(node);
  const call_t &call = call_node->get_call();

  return !fn_has_side_effects(call.function_name);
}

static bool check_obj(const Node *n0, const Node *n1,
                      const std::string &obj_name) {
  NodeType n0_type = n0->get_type();
  NodeType n1_type = n1->get_type();

  if (n0_type != n1_type || n0_type != NodeType::CALL) {
    return true;
  }

  const Call *n0_call_node = static_cast<const Call *>(n0);
  const Call *n1_call_node = static_cast<const Call *>(n1);

  const call_t &n0_call = n0_call_node->get_call();
  const call_t &n1_call = n1_call_node->get_call();

  auto n0_obj_it = n0_call.args.find(obj_name);
  auto n1_obj_it = n1_call.args.find(obj_name);

  if (n0_obj_it == n0_call.args.end() || n1_obj_it == n1_call.args.end()) {
    return false;
  }

  klee::ref<klee::Expr> n0_obj = n0_obj_it->second.expr;
  klee::ref<klee::Expr> n1_obj = n1_obj_it->second.expr;

  bool same_obj = kutil::solver_toolbox.are_exprs_always_equal(n0_obj, n1_obj);
  return same_obj;
}

static bool map_can_reorder(const BDD &bdd, const Node *anchor,
                            const Node *between, const Node *candidate,
                            klee::ref<klee::Expr> &condition) {
  if (!check_no_side_effects(candidate)) {
    return true;
  }

  if (!check_obj(between, candidate, "map")) {
    return true;
  }

  const klee::ConstraintManager &between_constraints =
      between->get_constraints();
  const klee::ConstraintManager &candidate_constraints =
      candidate->get_constraints();

  const Call *between_call_node = static_cast<const Call *>(between);
  const Call *candidate_call_node = static_cast<const Call *>(candidate);

  const call_t &between_call = between_call_node->get_call();
  const call_t &candidate_call = candidate_call_node->get_call();

  auto between_key_it = between_call.args.find("key");
  auto candidate_key_it = candidate_call.args.find("key");

  if (between_key_it == between_call.args.end() ||
      candidate_key_it == candidate_call.args.end()) {
    return false;
  }

  klee::ref<klee::Expr> between_key = between_key_it->second.in;
  klee::ref<klee::Expr> candidate_key = candidate_key_it->second.in;

  bool always_eq = kutil::solver_toolbox.are_exprs_always_equal(
      between_key, candidate_key, between_constraints, candidate_constraints);

  bool always_diff = kutil::solver_toolbox.are_exprs_always_not_equal(
      between_key, candidate_key, between_constraints, candidate_constraints);

  if (always_eq) {
    return false;
  }

  if (always_diff) {
    return true;
  }

  condition = kutil::solver_toolbox.exprBuilder->Not(
      kutil::solver_toolbox.exprBuilder->Eq(between_key, candidate_key));

  symbols_t anchor_symbols = bdd.get_generated_symbols(anchor);
  return io_check(condition, anchor_symbols);
}

static bool dchain_can_reorder(const Node *anchor, const Node *between,
                               const Node *candidate,
                               klee::ref<klee::Expr> &condition) {
  if (check_no_side_effects(candidate))
    return true;
  return !check_obj(between, candidate, "dchain");
}

static bool vector_can_reorder(const BDD &bdd, const Node *anchor,
                               const Node *between, const Node *candidate,
                               klee::ref<klee::Expr> &condition) {
  if (!check_no_side_effects(candidate)) {
    return true;
  }

  if (!check_obj(between, candidate, "vector")) {
    return true;
  }

  const klee::ConstraintManager &between_constraints =
      between->get_constraints();
  const klee::ConstraintManager &candidate_constraints =
      candidate->get_constraints();

  const Call *between_call_node = static_cast<const Call *>(between);
  const Call *candidate_call_node = static_cast<const Call *>(candidate);

  const call_t &between_call = between_call_node->get_call();
  const call_t &candidate_call = candidate_call_node->get_call();

  klee::ref<klee::Expr> between_index = between_call.args.at("index").expr;
  klee::ref<klee::Expr> candidate_index = candidate_call.args.at("index").expr;

  bool always_eq = kutil::solver_toolbox.are_exprs_always_equal(
      between_index, candidate_index, between_constraints,
      candidate_constraints);

  bool always_diff = kutil::solver_toolbox.are_exprs_always_not_equal(
      between_index, candidate_index, between_constraints,
      candidate_constraints);

  if (always_eq) {
    return false;
  }

  if (always_diff) {
    return true;
  }

  condition = kutil::solver_toolbox.exprBuilder->Not(
      kutil::solver_toolbox.exprBuilder->Eq(between_index, candidate_index));

  symbols_t anchor_symbols = bdd.get_generated_symbols(anchor);
  return io_check(condition, anchor_symbols);
}

static bool cht_can_reorder(const Node *anchor, const Node *between,
                            const Node *candidate,
                            klee::ref<klee::Expr> &condition) {
  return check_no_side_effects(candidate);
}

static bool sketch_can_reorder(const Node *anchor, const Node *between,
                               const Node *candidate,
                               klee::ref<klee::Expr> &condition) {
  bool can_reorder = true;
  can_reorder &= check_no_side_effects(candidate);
  can_reorder &= !check_obj(between, candidate, "sketch");
  return can_reorder;
}

static klee::ref<klee::Expr>
build_condition(const std::vector<klee::ref<klee::Expr>> &sub_conditions) {
  klee::ref<klee::Expr> condition;

  for (size_t i = 0; i < sub_conditions.size(); i++) {
    if (i == 0) {
      condition = sub_conditions[i];
    } else {
      condition =
          kutil::solver_toolbox.exprBuilder->And(condition, sub_conditions[i]);
    }
  }

  return condition;
}

static bool both_call_nodes(const Node *n0, const Node *n1) {
  return n0->get_type() == NodeType::CALL && n1->get_type() == NodeType::CALL;
}

static void
add_unique_condition(klee::ref<klee::Expr> new_condition,
                     std::vector<klee::ref<klee::Expr>> &conditions) {
  if (new_condition.isNull())
    return;

  for (const klee::ref<klee::Expr> &condition : conditions)
    if (kutil::solver_toolbox.are_exprs_always_equal(new_condition, condition))
      return;

  conditions.push_back(new_condition);
}

bool rw_check(const BDD &bdd, const Node *anchor, const Node *candidate,
              klee::ref<klee::Expr> &condition) {
  const Node *between = candidate->get_prev();

  std::vector<klee::ref<klee::Expr>> all_conditions;

  while (between != anchor) {

    if (!both_call_nodes(between, candidate)) {
      between = between->get_prev();
      continue;
    }

    klee::ref<klee::Expr> cond;
    bool can_reorder = true;
    can_reorder &= map_can_reorder(bdd, anchor, between, candidate, cond);
    can_reorder &= dchain_can_reorder(anchor, between, candidate, cond);
    can_reorder &= vector_can_reorder(bdd, anchor, between, candidate, cond);
    can_reorder &= cht_can_reorder(anchor, between, candidate, cond);
    can_reorder &= sketch_can_reorder(anchor, between, candidate, cond);

    if (!can_reorder) {
      return false;
    }

    add_unique_condition(cond, all_conditions);
    between = between->get_prev();
  }

  condition = build_condition(all_conditions);
  return true;
}

static bool anchor_reaches_candidate(const vector_t &anchor,
                                     const Node *candidate) {
  if (!candidate)
    return false;

  const Node *anchor_next = get_vector_next(anchor);

  if (!anchor_next)
    return false;

  node_id_t anchor_next_id = anchor_next->get_id();
  return candidate->is_reachable_by_node(anchor_next_id);
}

bool concretize_reordering_candidate(const BDD &bdd,
                                     const anchor_info_t &anchor_info,
                                     node_id_t proposed_candidate_id,
                                     candidate_info_t &candidate_info) {
  vector_t anchor;
  anchor.node = bdd.get_node_by_id(anchor_info.id);
  anchor.direction = anchor_info.direction;

  const Node *proposed_candidate = bdd.get_node_by_id(proposed_candidate_id);

  if (!anchor_reaches_candidate(anchor, proposed_candidate)) {
    return false;
  }

  symbols_t anchor_symbols = bdd.get_generated_symbols(anchor.node);

  assert(anchor.node && "Anchor node not found");
  assert(proposed_candidate && "Proposed candidate node not found");

  // No reordering if the proposed candidate is already following the anchor.
  if (get_vector_next(anchor) == proposed_candidate) {
    return false;
  }

  if (!io_check(proposed_candidate, anchor_symbols)) {
    return false;
  }

  switch (proposed_candidate->get_type()) {
  case NodeType::BRANCH: {
    // We can always reorder branches as long as the IO dependencies are met.
    bool find_in_all_branches = false;
    get_siblings(anchor, proposed_candidate, find_in_all_branches,
                 candidate_info.siblings);
  } break;
  case NodeType::CALL: {
    const Call *call_node = static_cast<const Call *>(proposed_candidate);
    const call_t &call = call_node->get_call();

    bool allow = true;
    allow &= fn_can_be_reordered(call.function_name);
    allow &= rw_check(bdd, anchor.node, call_node, candidate_info.condition);

    if (!allow) {
      return false;
    }

    bool find_in_all_branches = false;
    get_siblings(anchor, proposed_candidate, find_in_all_branches,
                 candidate_info.siblings);
  } break;
  case NodeType::ROUTE: {
    bool find_in_all_branches = true;
    if (!get_siblings(anchor, proposed_candidate, find_in_all_branches,
                      candidate_info.siblings)) {
      return false;
    }
  } break;
  }

  candidate_info.id = proposed_candidate_id;

  return true;
}

std::vector<candidate_info_t>
get_reordering_candidates(const BDD &bdd, const anchor_info_t &anchor_info) {
  std::vector<candidate_info_t> candidates;

  const Node *anchor = bdd.get_node_by_id(anchor_info.id);
  assert(anchor && "Anchor node not found");

  const Node *next = get_vector_next({anchor, anchor_info.direction});

  if (!next) {
    return candidates;
  }

  std::vector<const Node *> nodes{next};
  while (nodes.size()) {
    const Node *node = nodes[0];
    nodes.erase(nodes.begin());

    candidate_info_t proposed_candidate;
    bool success = concretize_reordering_candidate(
        bdd, anchor_info, node->get_id(), proposed_candidate);

    if (success) {
      candidates.push_back(proposed_candidate);
    }

    switch (node->get_type()) {
    case NodeType::BRANCH: {
      const Branch *branch_node = static_cast<const Branch *>(node);

      const Node *on_true = branch_node->get_on_true();
      const Node *on_false = branch_node->get_on_false();

      if (on_true)
        nodes.push_back(on_true);
      if (on_false)
        nodes.push_back(on_false);
    } break;
    case NodeType::CALL:
    case NodeType::ROUTE: {
      const Node *next = node->get_next();
      if (next)
        nodes.push_back(next);
    } break;
    }
  }

  return candidates;
}

// Returns the old next node.
static Node *link(const mutable_vector_t &anchor, Node *next) {
  Node *old_next = nullptr;

  switch (anchor.node->get_type()) {
  case NodeType::BRANCH: {
    Branch *branch = static_cast<Branch *>(anchor.node);
    if (anchor.direction) {
      old_next = branch->get_mutable_on_true();
      branch->set_on_true(next);
    } else {
      old_next = branch->get_mutable_on_false();
      branch->set_on_false(next);
    }
  } break;
  case NodeType::CALL:
  case NodeType::ROUTE:
    old_next = anchor.node->get_mutable_next();
    anchor.node->set_next(next);
    break;
  }

  if (next)
    next->set_prev(anchor.node);

  if (old_next)
    old_next->set_prev(nullptr);

  return old_next;
}

static void disconnect(Node *node) {
  Node *prev = node->get_mutable_prev();
  assert(prev && "Node has no previous node");

  bool direction = true;
  if (prev->get_type()) {
    Branch *prev_branch = static_cast<Branch *>(prev);
    if (prev_branch->get_on_true() == node) {
      direction = true;
    } else if (prev_branch->get_on_false() == node) {
      direction = false;
    } else {
      assert(false && "Node not found in previous branch node");
    }
  }

  switch (node->get_type()) {
  case NodeType::BRANCH: {
    Branch *branch = static_cast<Branch *>(node);
    branch->set_on_true(nullptr);
    branch->set_on_false(nullptr);
  } break;
  case NodeType::CALL:
  case NodeType::ROUTE:
    node->set_next(nullptr);
    break;
  }

  link({prev, direction}, nullptr);
}

static void disconnect_and_link_non_branch(Node *node) {
  assert(node->get_type() != NodeType::BRANCH);

  Node *next = node->get_mutable_next();
  Node *prev = node->get_mutable_prev();
  assert(prev && "Node has no previous node");

  bool direction = true;
  switch (prev->get_type()) {
  case NodeType::BRANCH: {
    Branch *prev_branch = static_cast<Branch *>(prev);
    if (prev_branch->get_on_true() == node) {
      direction = true;
    } else if (prev_branch->get_on_false() == node) {
      direction = false;
    } else {
      assert(false && "Node not found in previous branch node");
    }
  } break;
  case NodeType::CALL:
  case NodeType::ROUTE:
    direction = true;
    break;
  }

  link({prev, direction}, next);

  node->set_next(nullptr);
  node->set_prev(nullptr);
}

typedef std::unordered_map<Branch *, bool> directions_t;

static directions_t get_directions(const Node *anchor, Node *candidate) {
  directions_t directions;

  while (candidate != anchor) {
    Node *prev = candidate->get_mutable_prev();

    if (prev == anchor) {
      break;
    }

    if (prev->get_type() == NodeType::BRANCH) {
      Branch *prev_branch = static_cast<Branch *>(prev);

      Node *prev_on_true = prev_branch->get_mutable_on_true();
      Node *prev_on_false = prev_branch->get_mutable_on_false();

      if (prev_on_true == candidate) {
        directions[prev_branch] = true;
      } else if (prev_on_false == candidate) {
        directions[prev_branch] = false;
      }
    }

    candidate = prev;
  }

  return directions;
}

static std::unordered_map<Branch *, directions_t>
get_branch_candidates(const mutable_vector_t &anchor, Branch *candidate,
                      const std::unordered_set<Node *> &siblings) {
  std::unordered_map<Branch *, directions_t> branch_candidates;

  // Contrary to the non-branch case, here we need to consider all siblings
  // equally.
  std::unordered_set<Branch *> candidates{candidate};
  for (Node *sibling : siblings) {
    assert(sibling->get_type() == NodeType::BRANCH);
    candidates.insert(static_cast<Branch *>(sibling));
  }

  for (Branch *candidate : candidates) {
    branch_candidates[candidate] = get_directions(anchor.node, candidate);
  }

  return branch_candidates;
}

struct dangling_node_t {
  Node *node;
  directions_t directions;
  bool candidate_direction;
};

typedef std::vector<dangling_node_t> dangling_t;

static dangling_t
disconnect(const std::unordered_map<Branch *, directions_t> &candidates) {
  dangling_t dangling_nodes;

  for (const std::pair<Branch *, directions_t> &pair : candidates) {
    Branch *candidate = pair.first;
    directions_t directions = pair.second;

    Node *on_true = candidate->get_mutable_on_true();
    Node *on_false = candidate->get_mutable_on_false();

    if (on_true) {
      directions[candidate] = true;
      dangling_nodes.push_back({on_true, directions, true});
      on_true->set_prev(nullptr);
    }

    if (on_false) {
      directions[candidate] = false;
      dangling_nodes.push_back({on_false, directions, false});
      on_false->set_prev(nullptr);
    }

    disconnect(candidate);
  }

  return dangling_nodes;
}

typedef std::vector<mutable_vector_t> leaves_t;

static leaves_t get_leaves_from_candidates(
    const std::unordered_map<Branch *, directions_t> &candidates) {
  std::vector<mutable_vector_t> leaves;

  for (const std::pair<Branch *, directions_t> &pair : candidates) {
    Branch *candidate = pair.first;

    Node *prev = candidate->get_mutable_prev();
    assert(prev && "Candidate has no previous node");

    if (prev->get_type() == NodeType::BRANCH) {
      Branch *prev_branch = static_cast<Branch *>(prev);
      Node *prev_on_true = prev_branch->get_mutable_on_true();
      Node *prev_on_false = prev_branch->get_mutable_on_false();

      if (prev_on_true == candidate) {
        leaves.push_back({prev_branch, true});
      } else if (prev_on_false == candidate) {
        leaves.push_back({prev_branch, false});
      }
    } else {
      leaves.push_back({prev, true});
    }
  }

  return leaves;
}

static Node *clone_and_update_nodes(BDD &bdd, Node *candidate,
                                    Node *anchor_old_next, dangling_t &dangling,
                                    leaves_t &leaves) {
  NodeManager &manager = bdd.get_mutable_manager();
  node_id_t &id = bdd.get_mutable_id();

  Node *clone = anchor_old_next->clone(manager, true);

  for (mutable_vector_t &leaf : leaves) {
    node_id_t leaf_id = leaf.node->get_id();
    Node *leaf_clone = clone->get_mutable_node_by_id(leaf_id);
    leaves.push_back({leaf_clone, leaf.direction});
  }

  for (dangling_node_t &dangling_node : dangling) {
    // Only update nodes from the right side of the branch (that is the chosen
    // one to be the clone).
    if (dangling_node.candidate_direction)
      continue;

    // We are only interested in updating the directions.
    directions_t new_directions;

    for (const std::pair<Branch *, bool> &direction :
         dangling_node.directions) {
      // We don't clone the candidate.
      if (direction.first == candidate) {
        new_directions[direction.first] = direction.second;
        continue;
      }

      node_id_t branch_id = direction.first->get_id();
      Node *node_clone = clone->get_mutable_node_by_id(branch_id);

      std::cerr << "Branch node: " << direction.first->dump(true) << "\n";
      assert(node_clone && "Branch node not found in clone");

      Branch *branch_clone = static_cast<Branch *>(node_clone);
      new_directions[branch_clone] = direction.second;
    }

    dangling_node.directions = new_directions;
  }

  clone->recursive_update_ids(id);
  return clone;
}

static void filter_dangling(dangling_t &dangling, Branch *intersection,
                            bool decision) {
  auto wrong_decision = [intersection,
                         decision](const dangling_node_t &dangling_node) {
    auto found_it = dangling_node.directions.find(intersection);
    return found_it == dangling_node.directions.end() ||
           found_it->second != decision;
  };

  dangling.erase(
      std::remove_if(dangling.begin(), dangling.end(), wrong_decision),
      dangling.end());
}

static dangling_node_t dangling_from_leaf(Node *candidate, dangling_t dangling,
                                          const mutable_vector_t &leaf) {
  std::cerr << "\n====================\n";
  std::cerr << "Leaf node: " << leaf.node->dump(true) << " -> "
            << leaf.direction << "\n";
  std::cerr << "Dangling nodes:\n";
  for (const dangling_node_t &dangling_node : dangling) {
    std::cerr << "  " << dangling_node.node->dump(true) << "\n";
    std::cerr << "    directions:\n";
    for (const std::pair<Branch *, bool> &direction :
         dangling_node.directions) {
      std::cerr << "      " << direction.first->dump(true) << " -> "
                << direction.second << "\n";
    }
  }
  std::cerr << "====================\n";

  Node *node = leaf.node;

  if (leaf.node->get_type() == NodeType::BRANCH) {
    Branch *branch = static_cast<Branch *>(leaf.node);
    filter_dangling(dangling, branch, leaf.direction);
  }

  std::cerr << "\n====================\n";
  std::cerr << "**FILTERED**\n";
  std::cerr << "Leaf node: " << leaf.node->dump(true) << " -> "
            << leaf.direction << "\n";
  std::cerr << "Dangling nodes:\n";
  for (const dangling_node_t &dangling_node : dangling) {
    std::cerr << "  " << dangling_node.node->dump(true) << "\n";
    std::cerr << "    directions:\n";
    for (const std::pair<Branch *, bool> &direction :
         dangling_node.directions) {
      std::cerr << "      " << direction.first->dump(true) << " -> "
                << direction.second << "\n";
    }
  }
  std::cerr << "====================\n";

  while (node != candidate) {
    Node *prev = node->get_mutable_prev();
    assert(prev && "Node has no previous node");

    if (prev->get_type() == NodeType::BRANCH) {
      Branch *prev_branch = static_cast<Branch *>(prev);

      Node *prev_on_true = prev_branch->get_mutable_on_true();
      Node *prev_on_false = prev_branch->get_mutable_on_false();

      if (prev_on_true == node) {
        std::cerr << "Filter: " << prev_branch->dump(true) << " -> 1\n";
        filter_dangling(dangling, prev_branch, true);
      } else if (prev_on_false == node) {
        std::cerr << "Filter: " << prev_branch->dump(true) << " -> 0\n";
        filter_dangling(dangling, prev_branch, false);
      } else {
        assert(false && "Node not found in previous branch node");
      }

      std::cerr << "\n====================\n";
      std::cerr << "**FILTERED**\n";
      std::cerr << "Leaf node: " << leaf.node->dump(true) << " -> "
                << leaf.direction << "\n";
      std::cerr << "Dangling nodes:\n";
      for (const dangling_node_t &dangling_node : dangling) {
        std::cerr << "  " << dangling_node.node->dump(true) << "\n";
        std::cerr << "    directions:\n";
        for (const std::pair<Branch *, bool> &direction :
             dangling_node.directions) {
          std::cerr << "      " << direction.first->dump(true) << " -> "
                    << direction.second << "\n";
        }
      }
      std::cerr << "====================\n";
    }

    node = prev;
  }

  assert(dangling.size() == 1);
  return dangling[0];
}

static void stitch_dangling(BDD &bdd, Node *node, dangling_t dangling,
                            const leaves_t &leaves) {
  for (const mutable_vector_t &leaf : leaves) {
    dangling_node_t dangling_node = dangling_from_leaf(node, dangling, leaf);
    link(leaf, dangling_node.node);
  }
}

static void pull_branch(BDD &bdd, const mutable_vector_t &anchor,
                        Branch *candidate,
                        const std::unordered_set<Node *> &siblings) {
  assert(siblings.size() == 0 &&
         "TODO: implement branch reordering with siblings");

  std::cerr << "Reordering:\n";
  std::cerr << "  anchor: " << anchor.node->dump(true) << "\n";
  std::cerr << "  candidate: " << candidate->dump(true) << "\n";
  std::cerr << "  siblings: ";
  for (Node *sibling : siblings) {
    std::cerr << sibling->get_id() << " ";
  }
  std::cerr << "\n";
  std::cerr << "\n";

  std::unordered_map<Branch *, directions_t> branch_candidates =
      get_branch_candidates(anchor, candidate, siblings);

  std::cerr << "Branch candidates:\n";
  for (auto bc : branch_candidates) {
    std::cerr << "\t" << bc.first->dump(true) << "\n";
    for (auto d : bc.second) {
      std::cerr << "\t\t" << d.first->dump(true) << " -> " << d.second << "\n";
    }
  }
  std::cerr << "\n";

  leaves_t leaves = get_leaves_from_candidates(branch_candidates);
  dangling_t dangling = disconnect(branch_candidates);

  std::cerr << "Anchor: " << anchor.node->dump(true) << " -> "
            << anchor.direction << "\n";

  Node *anchor_old_next = link(anchor, candidate);

  std::cerr << "Old next: " << anchor_old_next->dump(true) << "\n";

  Node *anchor_next_clone =
      clone_and_update_nodes(bdd, candidate, anchor_old_next, dangling, leaves);

  std::cerr << "\n";
  std::cerr << "Dangling:\n";
  for (auto nd : dangling) {
    std::cerr << "\t" << nd.node->dump(true) << "\n";
    std::cerr << "\t\tcandidate direction: " << nd.candidate_direction << "\n";
    for (auto d : nd.directions) {
      std::cerr << "\t\t" << d.first->dump(true) << " -> " << d.second << "\n";
    }
  }
  std::cerr << "\n";

  std::cerr << "\n";
  std::cerr << "Leaves:\n";
  for (auto l : leaves) {
    std::cerr << "\t" << l.node->dump(true) << " -> " << l.direction << "\n";
  }
  std::cerr << "\n";

  assert(leaves.size() == dangling.size());

  link({candidate, true}, anchor_old_next);
  link({candidate, false}, anchor_next_clone);

  stitch_dangling(bdd, candidate, dangling, leaves);
}

static symbol_t get_collision_free_symbol(const symbols_t &symbols,
                                          const symbol_t &symbol) {
  const klee::Array *new_array;

  int suffix = 1;
  while (true) {
    std::string new_name = symbol.base + "_" + std::to_string(suffix);
    auto found_it = std::find_if(
        symbols.begin(), symbols.end(),
        [new_name](const symbol_t &s) { return s.array->name == new_name; });

    if (found_it == symbols.end()) {
      new_array = kutil::solver_toolbox.arr_cache.CreateArray(
          new_name, symbol.array->size,
          symbol.array->constantValues.begin().base(),
          symbol.array->constantValues.end().base(), symbol.array->domain,
          symbol.array->range);
      break;
    }

    suffix++;
  }

  symbol_t new_symbol;
  new_symbol.base = symbol.base;
  new_symbol.array = new_array;
  new_symbol.expr = kutil::solver_toolbox.create_new_symbol(new_symbol.array);

  return new_symbol;
}

static bool check_collision(const symbols_t &symbols, const symbol_t &symbol) {
  auto same_symbol = [symbol](const symbol_t &s) {
    return s.array->name == symbol.array->name;
  };
  auto found_it = std::find_if(symbols.begin(), symbols.end(), same_symbol);
  return found_it != symbols.end();
}

static void translate_symbols(BDD &bdd, const mutable_vector_t &anchor,
                              Node *candidate) {
  if (candidate->get_type() != NodeType::CALL)
    return;

  Call *candidate_call = static_cast<Call *>(candidate);
  symbols_t candidate_symbols = candidate_call->get_locally_generated_symbols();
  symbols_t anchor_symbols = bdd.get_generated_symbols(anchor.node);

  for (const symbol_t &candidate_symbol : candidate_symbols) {
    if (!check_collision(anchor_symbols, candidate_symbol))
      continue;
    symbol_t new_symbol =
        get_collision_free_symbol(anchor_symbols, candidate_symbol);
    candidate->recursive_translate_symbol(candidate_symbol, new_symbol);
  }
}

static void pull_non_branch(BDD &bdd, const mutable_vector_t &anchor,
                            Node *candidate,
                            const std::unordered_set<Node *> &siblings) {
  // Symbols generated by the candidate might collide with the symbols generated
  // until the anchor. In that case, we need to translate the candidate symbols
  // to avoid collisions.
  translate_symbols(bdd, anchor, candidate);

  // Remove candidate from the BDD, linking its parent with its child.
  disconnect_and_link_non_branch(candidate);

  // Link the anchor with the candidate.
  Node *anchor_old_next = link(anchor, candidate);
  link({candidate, true}, anchor_old_next);

  // Disconnect all siblings from the BDD.
  for (Node *sibling : siblings) {
    if (sibling == candidate)
      continue;
    disconnect_and_link_non_branch(sibling);
  }
}

static void pull_candidate(BDD &bdd, const mutable_vector_t &anchor,
                           Node *candidate,
                           const std::unordered_set<Node *> &siblings) {
  switch (candidate->get_type()) {
  case NodeType::BRANCH:
    pull_branch(bdd, anchor, static_cast<Branch *>(candidate), siblings);
    break;
  case NodeType::CALL:
  case NodeType::ROUTE:
    pull_non_branch(bdd, anchor, candidate, siblings);
    break;
  }
}

BDD reorder(const BDD &original_bdd, const anchor_info_t &anchor_info,
            const candidate_info_t &candidate_info) {
  BDD bdd = original_bdd;
  node_id_t &id = bdd.get_mutable_id();

  NodeManager &manager = bdd.get_mutable_manager();
  Node *candidate = bdd.get_mutable_node_by_id(candidate_info.id);

  mutable_vector_t anchor;
  anchor.node = bdd.get_mutable_node_by_id(anchor_info.id);
  anchor.direction = anchor_info.direction;

  std::unordered_set<Node *> siblings;
  for (node_id_t sibling_id : candidate_info.siblings) {
    Node *sibling = bdd.get_mutable_node_by_id(sibling_id);
    assert(sibling && "Sibling not found in BDD");
    siblings.insert(sibling);
  }

  assert(candidate && "Candidate not found in BDD");
  assert(anchor.node && "Anchor not found in BDD");

  // Reordering can only be done if a given extra condition is met.
  // Therefore, we must introduce a new branch condition evaluating this extra
  // condition.
  // Reordering will happen on the true side of this branch node, while the
  // false side will contain the original remaining nodes.
  if (!candidate_info.condition.isNull()) {
    klee::ConstraintManager anchor_constraints = anchor.node->get_constraints();

    Node *after_anchor = get_vector_next(anchor);
    assert(after_anchor && "Anchor has no next node");

    Node *after_anchor_clone = after_anchor->clone(manager, true);

    Branch *extra_branch =
        new Branch(id, anchor_constraints, candidate_info.condition);
    manager.add_node(extra_branch);
    id++;

    link(anchor, extra_branch);
    link({extra_branch, true}, after_anchor_clone);
    link({extra_branch, false}, after_anchor);

    // The candidate will be on the true side of the extra branch node.
    // We just cloned it, so we need to find it again.
    candidate = after_anchor_clone->get_mutable_node_by_id(candidate_info.id);
    assert(candidate && "Candidate not found in cloned after_anchor");

    // Same logic for all the siblings.
    siblings.clear();
    for (node_id_t sibling_id : candidate_info.siblings) {
      Node *sibling = bdd.get_mutable_node_by_id(sibling_id);
      assert(sibling && "Sibling not found in BDD");
      siblings.insert(sibling);
    }

    // Update the IDs of all cloned nodes (on true side of the extra branch).
    // WARNING: Now the information on the candidate info provided in
    // the arguments is not valid anymore (has the wrong candidate ID).
    after_anchor_clone->recursive_update_ids(id);

    // Finally update to the new anchor.
    anchor.node = extra_branch;
    anchor.direction = true;
  }

  pull_candidate(bdd, anchor, candidate, siblings);

  return bdd;
}

struct reordered_t {
  BDD bdd;
  std::vector<const Node *> next_nodes;
  int times;

  reordered_t(const BDD &_bdd, const Node *_next)
      : bdd(_bdd), next_nodes({_next}), times(0) {}

  reordered_t(const BDD &_bdd, std::vector<const Node *> _next_nodes,
              int _times)
      : bdd(_bdd), next_nodes(_next_nodes), times(_times) {}

  reordered_t(const BDD &_bdd, const Node *_on_true, const Node *_on_false,
              int _times)
      : bdd(_bdd), next_nodes({_on_true, _on_false}), times(_times) {}

  reordered_t(const reordered_t &other)
      : bdd(other.bdd), next_nodes(other.next_nodes), times(other.times) {}

  bool has_next() const { return next_nodes.size() > 0; }

  const Node *get_next() const {
    assert(has_next());
    return next_nodes[0];
  }

  void advance_next() {
    assert(has_next());

    const Node *next = next_nodes[0];
    next_nodes.erase(next_nodes.begin());

    if (next->get_type() == NodeType::BRANCH) {
      const Branch *branch_node = static_cast<const Branch *>(next);
      next_nodes.push_back(branch_node->get_on_true());
      next_nodes.push_back(branch_node->get_on_false());
    } else if (next->get_next()) {
      next_nodes.push_back(next->get_next());
    }
  }
};

// double approximate_total_reordered_bdds(const BDD &bdd, const Node *root) {
//   static std::unordered_map<std::string, double> cache;
//   static double total_max = 0;

//   double total = 0;

//   std::cerr << "Total ~ " << std::setprecision(2) << std::scientific
//             << total_max << "\r";

//   if (!root) {
//     return 0;
//   }

//   std::string hash = root->hash(true);

//   if (cache.find(hash) != cache.end()) {
//     return cache[hash];
//   }

//   NodeType type = root->get_type();

//   if (type == NodeType::BRANCH) {
//     const Branch *branch = static_cast<const Branch *>(root);

//     const Node *on_true = branch->get_on_true();
//     const Node *on_false = branch->get_on_false();

//     total += approximate_total_reordered_bdds(bdd, on_true);
//     total += approximate_total_reordered_bdds(bdd, on_false);
//   } else {
//     const Node *next = root->get_next();
//     total += approximate_total_reordered_bdds(bdd, next);
//   }

//   std::vector<reordered_bdd_t> reordered_bdds = reorder(bdd, root);

//   for (const reordered_bdd_t &reordered_bdd : reordered_bdds) {
//     total += approximate_total_reordered_bdds(reordered_bdd.bdd,
//                                               reordered_bdd.candidate);
//     total++;
//   }

//   cache[hash] = total;
//   total_max = std::max(total_max, total);

//   return total;
// }

// double approximate_total_reordered_bdds(const BDD &bdd) {
//   const Node *root = bdd.get_root();
//   double reordered = approximate_total_reordered_bdds(bdd, root);
//   double total = reordered + 1;
//   return total;
// }

} // namespace bdd