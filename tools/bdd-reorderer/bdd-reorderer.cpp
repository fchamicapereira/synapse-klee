#include "bdd-reorderer.h"

#include <iomanip>

namespace bdd {

// struct candidate_t {
//   const Node *node;
//   std::unordered_set<node_id_t> siblings;
//   klee::ref<klee::Expr> extra_condition;
//   klee::ref<klee::Expr> condition;

//   candidate_t(const Node *_node) : node(_node) {}

//   candidate_t(const Node *_node, klee::ref<klee::Expr> _condition)
//       : candidate_t(_node, _condition, false) {}

//   candidate_t(const Node *_node, klee::ref<klee::Expr> _condition, bool
//   _negate)
//       : node(_node) {
//     if (_negate) {
//       condition = kutil::solver_toolbox.exprBuilder->Not(_condition);
//     } else {
//       condition = _condition;
//     }
//   }

//   candidate_t(const candidate_t &candidate, const Node *_node)
//       : node(_node), condition(candidate.condition) {}

//   candidate_t(const candidate_t &candidate, const Node *_node,
//               klee::ref<klee::Expr> _condition)
//       : candidate_t(candidate, _node, _condition, false) {}

//   candidate_t(const candidate_t &candidate, const Node *_node,
//               klee::ref<klee::Expr> _condition, bool _negate)
//       : node(_node) {
//     klee::ref<klee::Expr> rhs;

//     if (_negate) {
//       rhs = kutil::solver_toolbox.exprBuilder->Not(_condition);
//     } else {
//       rhs = _condition;
//     }

//     if (!candidate.condition.isNull()) {
//       condition =
//           kutil::solver_toolbox.exprBuilder->And(candidate.condition, rhs);
//     } else {
//       condition = rhs;
//     }
//   }

//   bool has_sibling(node_id_t id) const {
//     return siblings.find(id) != siblings.end();
//   }

//   std::string dump() const {
//     std::stringstream stream;

//     stream << "\n";
//     stream << "  candidate : " << node->dump(true) << "\n";

//     if (node->get_type() == Node::NodeType::CALL) {
//       auto call_node = static_cast<Call *>(node.get());
//       auto symbols = call_node->get_locally_generated_symbols();

//       if (symbols.size()) {
//         stream << "  symbols   :";
//         for (auto symbol : symbols) {
//           stream << " " << symbol.label;
//         }
//         stream << "\n";
//       }
//     }

//     if (!condition.isNull()) {
//       stream << "  condition : " << kutil::expr_to_string(condition, true)
//              << "\n";
//     }

//     if (!extra_condition.isNull()) {
//       stream << "  extra condition : "
//              << kutil::expr_to_string(extra_condition, true) << "\n";
//     }
//     stream << "  siblings :  ";
//     for (auto s : siblings) {
//       stream << s << " ";
//     }
//     stream << "\n";

//     return stream.str();
//   }
// };

// operator<< for candidate_t
std::ostream &operator<<(std::ostream &stream, const candidate_t &candidate) {
  stream << "candidate: " << candidate.candidate_id;

  stream << "  siblings=[";
  bool first = false;
  for (node_id_t sibling : candidate.siblings) {
    if (!first) {
      stream << ", ";
    }
    stream << sibling;
    first = false;
  }
  stream << "]";

  if (!candidate.condition.isNull()) {
    stream << " (cond=" << kutil::expr_to_string(candidate.condition, true);
    stream << ")";
  }

  if (!candidate.extra_condition.isNull()) {
    stream << " (extra cond="
           << kutil::expr_to_string(candidate.extra_condition, true);
    stream << ")";
  }
  return stream;
}

static std::map<std::string, bool> fn_has_side_effects_lookup{
    {"current_time", true},
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
    "current_time",
    "nf_set_rte_ipv4_udptcp_checksum",
    "packet_borrow_next_chunk",
    "packet_return_chunk",
};

// bool fn_has_side_effects(std::string fn) {
//   auto found = fn_has_side_effects_lookup.find(fn);
//   if (found == fn_has_side_effects_lookup.end()) {
//     std::cerr << "ERROR: function \"" << fn
//               << "\" not in fn_has_side_effects_lookup\n";
//     assert(false && "TODO");
//   }
//   return found->second;
// }

// bool node_has_side_effects(const Node *node) {
//   if (node->get_type() == Node::NodeType::BRANCH) {
//     return true;
//   }

//   if (node->get_type() == Node::NodeType::CALL) {
//     auto fn = static_cast<const Call *>(node);
//     return fn_has_side_effects(fn->get_call().function_name);
//   }

//   return false;
// }

// bool fn_can_be_reordered(std::string fn) {
//   return std::find(fn_cannot_reorder_lookup.begin(),
//                    fn_cannot_reorder_lookup.end(),
//                    fn) == fn_cannot_reorder_lookup.end();
// }

bool read_in_chunk(klee::ref<klee::ReadExpr> read,
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

bool are_all_symbols_known(klee::ref<klee::Expr> expr,
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
    // if (SymbolFactory::should_ignore(symbol)) {
    //   continue;
    // }

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

static bool
is_called_in_all_future_branches(const Node *anchor, const Node *target,
                                 std::unordered_set<node_id_t> &siblings) {
  std::vector<const Node *> nodes = {anchor};
  Node::NodeType target_type = target->get_type();

  while (nodes.size()) {
    const Node *node = nodes[0];
    nodes.erase(nodes.begin());

    switch (node->get_type()) {
    case Node::NodeType::BRANCH: {
      const Branch *branch_node = static_cast<const Branch *>(node);

      if (target_type == Node::NodeType::BRANCH) {
        const Branch *target_branch = static_cast<const Branch *>(target);

        bool matching_conditions = kutil::solver_toolbox.are_exprs_always_equal(
            branch_node->get_condition(), target_branch->get_condition());

        if (matching_conditions) {
          siblings.insert(node->get_id());
          continue;
        }
      }

      nodes.push_back(branch_node->get_on_true());
      nodes.push_back(branch_node->get_on_false());
    } break;
    case Node::NodeType::CALL: {
      const Call *call_node = static_cast<const Call *>(node);

      if (target_type == Node::NodeType::CALL) {
        const Call *call_target = static_cast<const Call *>(target);

        bool matching_calls = kutil::solver_toolbox.are_calls_equal(
            call_node->get_call(), call_target->get_call());

        if (matching_calls) {
          siblings.insert(node->get_id());
          continue;
        }
      }

      if (!call_node->get_next()) {
        return false;
      }

      nodes.push_back(call_node->get_next());
    } break;
    case Node::NodeType::ROUTE: {
      const Route *route_node = static_cast<const Route *>(node);

      if (target_type == Node::NodeType::ROUTE) {
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

      if (!route_node->get_next()) {
        return false;
      }

      nodes.push_back(route_node->get_next());
    } break;
    }
  }

  return true;
}

static bool are_io_dependencies_met(const Node *node,
                                    const symbols_t &anchor_symbols) {
  bool met = true;

  switch (node->get_type()) {
  case Node::NodeType::BRANCH: {
    const Branch *branch_node = static_cast<const Branch *>(node);
    klee::ref<klee::Expr> condition = branch_node->get_condition();
    met &= are_all_symbols_known(condition, anchor_symbols);
  } break;
  case Node::NodeType::CALL: {
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
  case Node::NodeType::ROUTE:
    // Nothing to do here.
    break;
  }

  return met;
}

static bool are_io_dependencies_met(
    const Node *anchor, const Node *next,
    const std::unordered_set<node_id_t> &furthest_back_nodes) {
  symbols_t anchor_symbols = anchor->get_generated_symbols(furthest_back_nodes);
  return are_io_dependencies_met(next, anchor_symbols);
}

static bool are_io_dependencies_met(
    const Node *root, klee::ref<klee::Expr> expr,
    const std::unordered_set<node_id_t> &furthest_back_nodes) {
  symbols_t symbols = root->get_generated_symbols(furthest_back_nodes);
  return are_all_symbols_known(expr, symbols);
}

// bool map_can_reorder(const Node *current,
//                      const std::unordered_set<node_id_t>
//                      &furthest_back_nodes, const Node *before, const Node
//                      *after, klee::ref<klee::Expr> &condition) {
//   if (before->get_type() != after->get_type() ||
//       before->get_type() != Node::NodeType::CALL) {
//     return true;
//   }

//   auto before_constraints = before->get_constraints();
//   auto after_constraints = after->get_constraints();

//   auto before_call_node = static_cast<const Call *>(before);
//   auto after_call_node = static_cast<const Call *>(after);

//   auto before_call = before_call_node->get_call();
//   auto after_call = after_call_node->get_call();

//   auto before_map_it = before_call.args.find("map");
//   auto after_map_it = after_call.args.find("map");

//   if (before_map_it == before_call.args.end() ||
//       after_map_it == after_call.args.end()) {
//     return true;
//   }

//   auto before_map = before_map_it->second.expr;
//   auto after_map = after_map_it->second.expr;

//   assert(!before_map.isNull());
//   assert(!after_map.isNull());

//   if (!kutil::solver_toolbox.are_exprs_always_equal(before_map, after_map)) {
//     return true;
//   }

//   if (!fn_has_side_effects(before_call.function_name) &&
//       !fn_has_side_effects(after_call.function_name)) {
//     return true;
//   }

//   auto before_key_it = before_call.args.find("key");
//   auto after_key_it = after_call.args.find("key");

//   if (before_key_it == before_call.args.end() ||
//       after_key_it == after_call.args.end()) {
//     return false;
//   }

//   auto before_key = before_key_it->second.in;
//   auto after_key = after_key_it->second.in;

//   assert(!before_key.isNull());
//   assert(!after_key.isNull());

//   auto always_eq = kutil::solver_toolbox.are_exprs_always_equal(
//       before_key, after_key, before_constraints, after_constraints);

//   auto always_diff = kutil::solver_toolbox.are_exprs_always_not_equal(
//       before_key, after_key, before_constraints, after_constraints);

//   if (always_eq) {
//     return false;
//   }

//   if (always_diff) {
//     return true;
//   }

//   condition = kutil::solver_toolbox.exprBuilder->Not(
//       kutil::solver_toolbox.exprBuilder->Eq(before_key, after_key));

//   return are_io_dependencies_met(before, condition, furthest_back_nodes);
// }

// bool dchain_can_reorder(
//     const Node *current,
//     const std::unordered_set<node_id_t> &furthest_back_nodes,
//     const Node *before, const Node *after, klee::ref<klee::Expr> &condition)
//     {
//   if (before->get_type() != after->get_type() ||
//       before->get_type() != Node::NodeType::CALL) {
//     return true;
//   }

//   auto before_constraints = before->get_constraints();
//   auto after_constraints = after->get_constraints();

//   auto before_call_node = static_cast<const Call *>(before);
//   auto after_call_node = static_cast<const Call *>(after);

//   auto before_call = before_call_node->get_call();
//   auto after_call = after_call_node->get_call();

//   if (!fn_has_side_effects(before_call.function_name) &&
//       !fn_has_side_effects(after_call.function_name)) {
//     return true;
//   }

//   auto before_dchain_it = before_call.args.find("dchain");
//   auto after_dchain_it = after_call.args.find("dchain");

//   if (before_dchain_it == before_call.args.end() ||
//       after_dchain_it == after_call.args.end()) {
//     return true;
//   }

//   auto before_dchain = before_dchain_it->second.expr;
//   auto after_dchain = after_dchain_it->second.expr;

//   assert(!before_dchain.isNull());
//   assert(!after_dchain.isNull());

//   if (!kutil::solver_toolbox.are_exprs_always_equal(before_dchain,
//                                                     after_dchain)) {
//     return true;
//   }

//   return false;
// }

// bool vector_can_reorder(
//     const Node *current,
//     const std::unordered_set<node_id_t> &furthest_back_nodes,
//     const Node *before, const Node *after, klee::ref<klee::Expr> &condition)
//     {
//   if (before->get_type() != after->get_type() ||
//       before->get_type() != Node::NodeType::CALL) {
//     return true;
//   }

//   auto before_constraints = before->get_constraints();
//   auto after_constraints = after->get_constraints();

//   auto before_call_node = static_cast<const Call *>(before);
//   auto after_call_node = static_cast<const Call *>(after);

//   auto before_call = before_call_node->get_call();
//   auto after_call = after_call_node->get_call();

//   if (!fn_has_side_effects(before_call.function_name) &&
//       !fn_has_side_effects(after_call.function_name)) {
//     return true;
//   }

//   auto before_vector_it = before_call.args.find("vector");
//   auto after_vector_it = after_call.args.find("vector");

//   if (before_vector_it == before_call.args.end() ||
//       after_vector_it == after_call.args.end()) {
//     return true;
//   }

//   auto before_vector = before_vector_it->second.expr;
//   auto after_vector = after_vector_it->second.expr;

//   assert(!before_vector.isNull());
//   assert(!after_vector.isNull());

//   if (!kutil::solver_toolbox.are_exprs_always_equal(before_vector,
//                                                     after_vector)) {
//     return true;
//   }

//   auto before_index_it = before_call.args.find("index");
//   auto after_index_it = after_call.args.find("index");

//   auto before_index = before_index_it->second.expr;
//   auto after_index = after_index_it->second.expr;

//   assert(!before_index.isNull());
//   assert(!after_index.isNull());

//   auto always_eq = kutil::solver_toolbox.are_exprs_always_equal(
//       before_index, after_index, before_constraints, after_constraints);

//   auto always_diff = kutil::solver_toolbox.are_exprs_always_not_equal(
//       before_index, after_index, before_constraints, after_constraints);

//   if (always_eq) {
//     return false;
//   }

//   if (always_diff) {
//     return true;
//   }

//   condition = kutil::solver_toolbox.exprBuilder->Not(
//       kutil::solver_toolbox.exprBuilder->Eq(before_index, after_index));

//   return are_io_dependencies_met(current, condition, furthest_back_nodes);
// }

// bool are_rw_dependencies_met(
//     const Node *root, const Node *next_node,
//     const std::unordered_set<node_id_t> &furthest_back_nodes,
//     klee::ref<klee::Expr> &condition) {
//   assert(root);
//   auto node = next_node->get_prev();
//   assert(node);

//   std::vector<klee::ref<klee::Expr>> all_conditions;

//   while (node->get_id() != root->get_id()) {
//     klee::ref<klee::Expr> local_condition;

//     if (!map_can_reorder(root, furthest_back_nodes, node.get(), next_node,
//                          local_condition)) {
//       return false;
//     }

//     if (!dchain_can_reorder(root, furthest_back_nodes, node.get(), next_node,
//                             local_condition)) {
//       return false;
//     }

//     if (!vector_can_reorder(root, furthest_back_nodes, node.get(), next_node,
//                             local_condition)) {
//       return false;
//     }

//     // TODO: missing cht and sketch

//     if (!local_condition.isNull()) {
//       all_conditions.push_back(local_condition);
//     }

//     node = node->get_prev();
//     assert(node);
//   }

//   if (all_conditions.size() == 0) {
//     return true;
//   }

//   condition = all_conditions[0];

//   all_conditions.pop_back();
//   while (all_conditions.size()) {
//     condition =
//         kutil::solver_toolbox.exprBuilder->And(condition, all_conditions[0]);
//     all_conditions.pop_back();
//   }

//   return true;
// }

bool concretize_reordering_candidate(const BDD &bdd, node_id_t anchor_id,
                                     node_id_t proposed_candidate_id,
                                     candidate_t &candidate) {
  const Node *anchor = bdd.get_node_by_id(anchor_id);
  const Node *proposed_candidate = bdd.get_node_by_id(proposed_candidate_id);

  assert(anchor && "Anchor node not found");
  assert(proposed_candidate && "Proposed candidate node not found");

  symbols_t anchor_symbols = anchor->get_generated_symbols();

  if (!are_io_dependencies_met(proposed_candidate, anchor_symbols)) {
    std::cerr << "IO dependencies not met\n";
    return false;
  }

  switch (proposed_candidate->get_type()) {
  case Node::NodeType::BRANCH: {
    assert(false && "TODO");
  } break;
  case Node::NodeType::CALL: {
    assert(false && "TODO");
  } break;
  case Node::NodeType::ROUTE: {
    if (!is_called_in_all_future_branches(anchor, proposed_candidate,
                                          candidate.siblings)) {
      std::cerr << "Not called in all future branches\n";
      return false;
    }
  } break;
  }

  candidate.candidate_id = proposed_candidate_id;

  return true;
}

// static std::vector<candidate_t>
// get_candidates(const Node *anchor,
//                const std::unordered_set<node_id_t> &furthest_back_nodes) {
//   std::vector<candidate_t> viable_candidates;
//   std::vector<candidate_t> candidates;

//   if (!anchor->get_next() || !anchor->get_next()->get_next() ||
//       anchor->get_type() == Node::BRANCH) {
//     return candidates;
//   }

//   auto check_future_branches = false;
//   auto next = anchor->get_next();

//   if (next->get_type() == Node::BRANCH) {
//     auto branch = static_cast<const Branch *>(next);
//     candidates.emplace_back(branch->get_on_true(), branch->get_condition());
//     candidates.emplace_back(branch->get_on_false(), branch->get_condition(),
//                             true);
//     check_future_branches = true;
//   } else {
//     candidates.emplace_back(next->get_next());
//   }

//   while (candidates.size()) {
//     candidate_t candidate(candidates[0]);
//     candidates.erase(candidates.begin());

//     if (candidate.node->get_type() == Node::BRANCH) {
//       auto branch = static_cast<const Branch *>(candidate.node);
//       check_future_branches = true;

//       candidates.emplace_back(candidate, branch->get_on_true(),
//                               branch->get_condition());
//       candidates.emplace_back(candidate, branch->get_on_false(),
//                               branch->get_condition(), true);
//     } else if (candidate.node->get_next()) {
//       candidates.emplace_back(candidate, candidate.node->get_next());
//     }

//     auto found_it =
//         std::find_if(viable_candidates.begin(), viable_candidates.end(),
//                      [&](candidate_t c) -> bool {
//                        auto found_it =
//                            std::find(c.siblings.begin(), c.siblings.end(),
//                                      candidate.node->get_id());
//                        return found_it != c.siblings.end();
//                      });

//     if (found_it != viable_candidates.end()) {
//       continue;
//     }

//     if (!are_io_dependencies_met(anchor, candidate.node,
//     furthest_back_nodes)) {
//       continue;
//     }

//     if (candidate.node->get_type() == Node::NodeType::CALL) {
//       auto candidate_call = static_cast<const Call *>(candidate.node);

//       if (!fn_can_be_reordered(candidate_call->get_call().function_name)) {
//         continue;
//       }

//       if (!are_rw_dependencies_met(anchor, candidate.node,
//       furthest_back_nodes,
//                                    candidate.extra_condition)) {
//         continue;
//       }
//     }

//     auto viable = !check_future_branches ||
//                   !node_has_side_effects(candidate.node) ||
//                   is_called_in_all_future_branches(anchor, candidate.node,
//                                                    candidate.siblings);

//     if (!viable) {
//       continue;
//     }

//     candidate.siblings.insert(candidate.node->get_id());
//     viable_candidates.push_back(candidate);
//   }

//   return viable_candidates;
// }

// BDD reorder(const BDD &original_bdd, node_id_t anchor_id,
//             const candidate_t &candidate) {
//   struct aux_t {
//     const Node *node;
//     bool branch_decision;
//     bool branch_decision_set;

//     aux_t(const Node *_node) : node(_node), branch_decision_set(false) {}
//     aux_t(const Node *_node, bool _direction)
//         : node(_node), branch_decision(_direction), branch_decision_set(true)
//         {}
//   };

//   BDD bdd = original_bdd;

//   node_id_t candidate_id = candidate.node->get_id();

//   const Node *anchor = bdd.get_node_by_id(anchor_id);
//   const Node *clone_candidate = bdd.get_node_by_id(candidate_id);

//   assert(clone_anchor);
//   assert(clone_candidate);

//   candidate.node = clone_candidate;

//   auto id = bdd.get_id();
//   std::vector<aux_t> leaves;
//   auto candidate_clone = candidate.node->clone();

//   auto old_next = anchor->get_next();
//   assert(anchor->get_type() != Node::BRANCH);
//   assert(old_next);

//   if (!candidate.extra_condition.isNull()) {
//     klee::ConstraintManager no_constraints;

//     auto old_next_cloned = old_next->clone(true);

//     old_next_cloned->recursive_update_ids(++id);
//     bdd.set_id(id);

//     auto branch =
//         std::make_shared<Branch>(id, no_constraints,
//         candidate.extra_condition);

//     bdd.set_id(++id);

//     branch->replace_on_true(candidate_clone);
//     branch->replace_on_false(old_next_cloned);

//     candidate_clone->replace_prev(branch);
//     old_next_cloned->replace_prev(branch);

//     root->replace_next(branch);
//     branch->replace_prev(root);
//   } else {
//     root->replace_next(candidate_clone);
//     candidate_clone->replace_prev(root);
//   }

//   if (candidate_clone->get_type() == Node::NodeType::BRANCH) {
//     auto branch = static_cast<Branch *>(candidate_clone.get());

//     auto old_next_on_true = old_next;
//     auto old_next_on_false = old_next->clone(true);

//     branch->replace_on_true(old_next_on_true);
//     branch->replace_on_false(old_next_on_false);

//     old_next_on_true->replace_prev(candidate_clone);
//     old_next_on_false->replace_prev(candidate_clone);

//     leaves.emplace_back(old_next_on_true, true);
//     leaves.emplace_back(old_next_on_false, false);
//   } else {
//     candidate_clone->replace_next(old_next);
//     old_next->replace_prev(candidate_clone);

//     leaves.emplace_back(old_next);
//   }

//   auto node = anchor;
//   while (leaves.size()) {
//     node = leaves[0].node;

//     if (!node || !node->get_next()) {
//       leaves.erase(leaves.begin());
//       continue;
//     }

//     if (node->get_type() == Node::NodeType::BRANCH) {
//       auto branch = static_cast<Branch *>(node.get());

//       auto on_true = branch->get_on_true();
//       auto on_false = branch->get_on_false();

//       auto found_on_true = candidate.has_sibling(on_true->get_id());
//       auto found_on_false = candidate.has_sibling(on_false->get_id());

//       if (found_on_true) {
//         const Node *next;

//         if (on_true->get_type() == Node::NodeType::BRANCH) {
//           auto on_true_branch = static_cast<Branch *>(on_true.get());
//           assert(leaves[0].branch_decision_set);
//           next = leaves[0].branch_decision ? on_true_branch->get_on_true()
//                                            : on_true_branch->get_on_false();
//         } else {
//           next = on_true->get_next();
//         }

//         branch->replace_on_true(next);
//         next->replace_prev(node);
//       }

//       if (found_on_false) {
//         const Node *next;

//         if (on_false->get_type() == Node::NodeType::BRANCH) {
//           auto on_false_branch = static_cast<Branch *>(on_false.get());
//           assert(leaves[0].branch_decision_set);
//           next = leaves[0].branch_decision ? on_false_branch->get_on_true()
//                                            : on_false_branch->get_on_false();
//         } else {
//           next = on_false->get_next();
//         }

//         branch->replace_on_false(next);
//         next->replace_prev(node);
//       }

//       auto branch_decision = leaves[0].branch_decision;
//       leaves.erase(leaves.begin());

//       leaves.emplace_back(branch->get_on_true(), branch_decision);
//       leaves.emplace_back(branch->get_on_false(), branch_decision);
//     } else {
//       auto next = node->get_next();
//       auto found_sibling = candidate.has_sibling(next->get_id());

//       if (found_sibling) {
//         const Node *next_next;

//         if (next->get_type() == Node::NodeType::BRANCH) {
//           auto next_branch = static_cast<Branch *>(next.get());
//           assert(leaves[0].branch_decision_set);
//           next_next = leaves[0].branch_decision ? next_branch->get_on_true()
//                                                 :
//                                                 next_branch->get_on_false();
//         } else {
//           next_next = next->get_next();
//         }

//         node->replace_next(next_next);
//         next_next->replace_prev(node);

//         next = next_next;
//       }

//       leaves[0].node = next;
//     }
//   }

//   if (candidate_clone->get_type() == Node::NodeType::BRANCH) {
//     auto branch = static_cast<Branch *>(candidate_clone.get());
//     auto on_false = branch->get_on_false();

//     auto id = bdd.get_id();
//     on_false->recursive_update_ids(++id);
//     bdd.set_id(id);
//   }
// }

// std::vector<reordered_bdd_t> reorder(const BDD &bdd, node_id_t anchor_id) {
//   const Node *root = bdd.get_root();
//   node_id_t root_id = root->get_id();
//   std::unordered_set<node_id_t> furthest_back_nodes{root_id};
//   return reorder(bdd, anchor_id, furthest_back_nodes);
// }

// std::vector<reordered_bdd_t>
// reorder(const BDD &bdd, node_id_t anchor_id,
//         const std::unordered_set<node_id_t> &furthest_back_nodes) {
//   std::vector<reordered_bdd_t> reordered;

//   std::vector<candidate_t> candidates =
//       get_candidates(anchor, furthest_back_nodes);

//   // #ifndef NDEBUG
//   //   std::cerr << "\n";
//   //   std::cerr <<
//   "*********************************************************"
//   //                "********************\n";
//   //   std::cerr << "  current   : " << root->dump(true) << "\n";
//   //   for (auto candidate : candidates) {
//   //     std::cerr << candidate.dump() << "\n";
//   //   }
//   //   std::cerr <<
//   "*********************************************************"
//   //                "********************\n";
//   // #endif

//   for (const candidate_t &candidate : candidates) {
//     BDD bdd_clone = reorder(bdd, anchor_id, candidate);

//     node_id_t candidate_id = candidate.node->get_id();
//     const Node *clone_candidate = bdd_clone.get_node_by_id(candidate_id);
//     assert(clone_candidate);

//     reordered.emplace_back(bdd_clone, clone_candidate, candidate.condition);
//   }

//   return reordered;
// }

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

    if (next->get_type() == Node::NodeType::BRANCH) {
      const Branch *branch_node = static_cast<const Branch *>(next);
      next_nodes.push_back(branch_node->get_on_true());
      next_nodes.push_back(branch_node->get_on_false());
    } else if (next->get_next()) {
      next_nodes.push_back(next->get_next());
    }
  }
};

// std::vector<BDD> get_all_reordered_bdds(const BDD &original_bdd,
//                                         int max_reordering) {
//   std::vector<BDD> result;

//   const Node *root = original_bdd.get_root();
//   std::vector<reordered_t> bdds{{original_bdd, root}};

//   while (bdds.size()) {
//     reordered_t bdd = bdds[0];
//     bdds.erase(bdds.begin());

//     if (!bdd.has_next() ||
//         (max_reordering >= 0 && bdd.times >= max_reordering)) {
//       result.emplace_back(bdd.bdd);

// #ifndef NDEBUG
//       std::cerr << "\r"
//                 << "completed: " << result.size() << std::flush;
// #endif

//       continue;
//     }

//     std::vector<reordered_bdd_t> reordered_bdds =
//         reorder(bdd.bdd, bdd.get_next());

//     for (const reordered_bdd_t &reordered_bdd : reordered_bdds) {
//       std::vector<const Node *> new_nexts;
//       for (const Node *next : bdd.next_nodes) {
//         node_id_t next_id = next->get_id();
//         const Node *next_in_reordered =
//             reordered_bdd.bdd.get_node_by_id(next_id);
//         new_nexts.push_back(next_in_reordered);
//       }

//       reordered_t new_reordered(reordered_bdd.bdd, new_nexts, bdd.times + 1);
//       new_reordered.advance_next();
//       bdds.push_back(new_reordered);
//     }

//     bdd.advance_next();
//     bdds.push_back(bdd);
//   }

//   return result;
// }

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

//   Node::NodeType type = root->get_type();

//   if (type == Node::NodeType::BRANCH) {
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