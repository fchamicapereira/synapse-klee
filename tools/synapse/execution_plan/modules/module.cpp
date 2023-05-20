#include "../../../bdd-reorderer/bdd-reorderer.h"

#include "../execution_plan.h"
#include "../visitors/graphviz/graphviz.h"
#include "../visitors/visitor.h"
#include "module.h"

#include <algorithm>

using synapse::symbex::CHUNK;
using synapse::symbex::PACKET_LENGTH;

namespace synapse {

std::vector<ExecutionPlan> get_reordered(const ExecutionPlan &ep,
                                         int max_reordered) {
  std::vector<ExecutionPlan> reordered;

  if (max_reordered >= 0 && (int)ep.get_reordered_nodes() >= max_reordered) {
    return reordered;
  }

  auto next_node = ep.get_next_node();

  if (!next_node) {
    return reordered;
  }

  auto current_node = next_node->get_prev();

  if (!current_node) {
    return reordered;
  }

  auto current_bdd = ep.get_bdd();
  auto current_target = ep.get_current_platform();
  auto starting_points_of_targets = ep.get_targets_bdd_starting_points();
  auto current_starting_points = starting_points_of_targets[current_target];

  auto reordered_bdds =
      BDD::reorder(current_bdd, current_node, current_starting_points);

  for (auto reordered_bdd : reordered_bdds) {
    auto ep_cloned = ep.clone(reordered_bdd.bdd);

    if (!reordered_bdd.condition.isNull()) {
      auto mb = ep_cloned.get_memory_bank();
      mb->add_reorder_data(reordered_bdd.candidate->get_id(),
                           reordered_bdd.condition);
    }

    ep_cloned.replace_active_leaf_node(reordered_bdd.candidate, false);
    ep_cloned.inc_reordered_nodes();

    // If the next node was a BDD starting point, then actually the starting
    // point becomes the candidate node.
    ep_cloned.replace_current_target_starting_points(
        next_node->get_id(), reordered_bdd.candidate->get_id());

    reordered.push_back(ep_cloned);
  }

  return reordered;
}

bool can_process_platform(const ExecutionPlan &ep, TargetType target) {
  auto current_target = ep.get_current_platform();
  return current_target == target;
}

processing_result_t Module::process_node(const ExecutionPlan &ep,
                                         BDD::Node_ptr node,
                                         int max_reordered) {
  assert(node);
  processing_result_t result;

  if (can_process_platform(ep, target)) {
    result = process(ep, node);
  }

  std::vector<ExecutionPlan> reordered;

  for (auto ep : result.next_eps) {
    auto ep_reodered = get_reordered(ep, max_reordered);
    reordered.insert(reordered.end(), ep_reodered.begin(), ep_reodered.end());
  }

  if (reordered.size() > 0) {
    Log::dbg() << "+ " << reordered.size() << " reordered BDDs\n";
  }

  result.next_eps.insert(result.next_eps.end(), reordered.begin(),
                         reordered.end());

  return result;
}

bool Module::query_contains_map_has_key(const BDD::Branch *node) const {
  assert(!node->get_condition().isNull());
  auto _condition = node->get_condition();

  kutil::RetrieveSymbols retriever;
  retriever.visit(_condition);

  auto symbols = retriever.get_retrieved_strings();

  auto found_it = std::find_if(
      symbols.begin(), symbols.end(), [](const std::string &symbol) -> bool {
        return symbol.find(symbex::MAP_HAS_THIS_KEY) != std::string::npos;
      });

  if (found_it == symbols.end()) {
    return false;
  }

  return true;
}

std::vector<BDD::Node_ptr> Module::get_prev_fn(
    const ExecutionPlan &ep, BDD::Node_ptr node,
    const std::vector<std::string> &functions_names) const {
  std::vector<BDD::Node_ptr> prev_functions;

  auto target = ep.get_current_platform();

  auto targets_bdd_starting_points = ep.get_targets_bdd_starting_points();
  auto starting_points_it = targets_bdd_starting_points.find(target);

  auto is_starting_point = [&](const BDD::Node_ptr &node) -> bool {
    if (starting_points_it == targets_bdd_starting_points.end()) {
      return false;
    }

    auto starting_points = starting_points_it->second;
    return starting_points.find(node->get_id()) != starting_points.end();
  };

  assert(node);

  while (node && (node = node->get_prev()) && !is_starting_point(node)) {
    if (node->get_type() != BDD::Node::NodeType::CALL) {
      continue;
    }

    auto call_node = static_cast<const BDD::Call *>(node.get());
    auto call = call_node->get_call();

    auto found_it = std::find(functions_names.begin(), functions_names.end(),
                              call.function_name);

    if (found_it != functions_names.end()) {
      prev_functions.push_back(node);
    }

    // TODO: this is a hack to deal with consolidated BDDs
    if(call.function_name == "current_time") {
      break;
    }
  }

  return prev_functions;
}

std::vector<BDD::Node_ptr>
Module::get_prev_fn(const ExecutionPlan &ep, BDD::Node_ptr node,
                               const std::string &function_name) const {
  auto functions_names = std::vector<std::string>{function_name};
  return get_prev_fn(ep, node, functions_names);
}

std::vector<Module_ptr>
Module::get_prev_modules(const ExecutionPlan &ep,
                         const std::vector<ModuleType> &targets) const {
  std::vector<Module_ptr> modules;
  auto ep_node = ep.get_active_leaf();

  while (ep_node) {
    auto current = ep_node;
    ep_node = current->get_prev();

    auto module = current->get_module();
    assert(module);

    auto type = module->get_type();

    auto found_it = std::find(targets.begin(), targets.end(), type);

    if (found_it != targets.end()) {
      modules.push_back(module);
    }
  }

  return modules;
}

std::vector<Module::modification_t>
Module::build_modifications(klee::ref<klee::Expr> before,
                            klee::ref<klee::Expr> after) const {
  std::vector<modification_t> _modifications;
  assert(before->getWidth() == after->getWidth());

  auto size = before->getWidth();

  for (unsigned int b = 0; b < size; b += 8) {
    auto before_byte =
        kutil::solver_toolbox.exprBuilder->Extract(before, b, klee::Expr::Int8);
    auto after_byte =
        kutil::solver_toolbox.exprBuilder->Extract(after, b, klee::Expr::Int8);

    if (kutil::solver_toolbox.are_exprs_always_equal(before_byte, after_byte)) {
      continue;
    }

    _modifications.emplace_back(b / 8, after_byte);
  }

  return _modifications;
}

/*
  We use this method to check if we can coalesce the Map + Vector + Dchain
  paradigm into a single data structure mimicking a common map (which
  maps arbitrary data to arbitrary values).

  These data structures should coalesce if the index allocated by the dchain
  is used as a map value and a vector index.

  Multiple vectors can be coalesced into the same data structure, but typically
  a single map and a single dchain are used.

  The pattern we are looking is the following:

  1. Allocating the index
  -> dchain_allocate_new_index(dchain, &index)

  2. Storing the key
  -> vector_borrow(vector_1, index, value_1)

  3. Updating the map
  -> map_put(map_1, key, index)

  4. Returnin the key
  -> vector_return(vector_1, index, value_1)

  [ Loop updating all the other vectors ]
  -> vector_borrow(vector_n, index, value_n)
  -> vector_return(vector_n, index, value_n)
*/

struct next_t {
  std::vector<BDD::Node_ptr> maps;
  std::vector<obj_addr_t> maps_addrs;

  std::vector<BDD::Node_ptr> vectors;
  std::vector<obj_addr_t> vectors_addrs;

  int size() const { return maps_addrs.size() + vectors_addrs.size(); }

  void intersect(const next_t &other) {
    auto i = 0u;

    while (i < maps_addrs.size()) {
      auto current_addr = maps_addrs[i];
      auto found_it = std::find(other.maps_addrs.begin(),
                                other.maps_addrs.end(), current_addr);

      if (found_it == other.maps_addrs.end()) {
        maps.erase(maps.begin() + i);
        maps_addrs.erase(maps_addrs.begin() + i);
      } else {
        i++;
      }
    }

    i = 0;

    while (i < vectors_addrs.size()) {
      auto current_addr = vectors_addrs[i];
      auto found_it = std::find(other.vectors_addrs.begin(),
                                other.vectors_addrs.end(), current_addr);

      if (found_it == other.vectors_addrs.end()) {
        vectors.erase(vectors.begin() + i);
        vectors_addrs.erase(vectors_addrs.begin() + i);
      } else {
        i++;
      }
    }
  }
};

next_t get_next_maps_and_vectors_ops(BDD::Node_ptr root, obj_addr_t map_addr,
                                     klee::ref<klee::Expr> index) {
  assert(root);

  std::vector<BDD::Node_ptr> nodes{root};
  next_t candidates;

  while (nodes.size()) {
    auto node = nodes[0];
    nodes.erase(nodes.begin());

    if (node->get_type() == BDD::Node::BRANCH) {
      auto branch_node = static_cast<const BDD::Branch *>(node.get());
      nodes.push_back(branch_node->get_on_true());
      nodes.push_back(branch_node->get_on_false());
      continue;
    }

    if (node->get_type() != BDD::Node::CALL) {
      continue;
    }

    auto call_node = static_cast<const BDD::Call *>(node.get());
    auto call = call_node->get_call();

    if (call.function_name == symbex::FN_MAP_PUT) {
      auto _map = call.args[symbex::FN_MAP_ARG_MAP].expr;
      auto _value = call.args[symbex::FN_MAP_ARG_VALUE].expr;

      auto _map_addr = kutil::expr_addr_to_obj_addr(_map);
      auto same_index =
          kutil::solver_toolbox.are_exprs_always_equal(index, _value);

      if (map_addr == _map_addr && same_index) {
        candidates.maps.push_back(node);
        candidates.maps_addrs.push_back(_map_addr);
      }
    }

    else if (call.function_name == symbex::FN_VECTOR_BORROW) {
      auto _vector = call.args[symbex::FN_VECTOR_ARG_VECTOR].expr;
      auto _value = call.args[symbex::FN_VECTOR_ARG_INDEX].expr;

      auto _vector_addr = kutil::expr_addr_to_obj_addr(_vector);
      auto same_index =
          kutil::solver_toolbox.are_exprs_always_equal(index, _value);

      if (same_index) {
        candidates.vectors.push_back(node);
        candidates.vectors_addrs.push_back(_vector_addr);
      }
    }

    nodes.push_back(node->get_next());
  }

  return candidates;
}

bool Module::is_expr_only_packet_dependent(klee::ref<klee::Expr> expr) const {
  kutil::RetrieveSymbols retriever;
  retriever.visit(expr);

  auto symbols = retriever.get_retrieved_strings();

  std::vector<std::string> allowed_symbols = {
      symbex::PACKET_LENGTH,
      symbex::CHUNK,
  };

  for (auto symbol : symbols) {
    auto found_it =
        std::find(allowed_symbols.begin(), allowed_symbols.end(), symbol);

    if (found_it == allowed_symbols.end()) {
      return false;
    }
  }

  return true;
}

std::vector<BDD::Node_ptr>
Module::get_all_functions_after_node(BDD::Node_ptr root,
                                     const std::string &function_name,
                                     bool stop_on_branches) const {
  assert(root);

  std::vector<BDD::Node_ptr> functions;
  std::vector<BDD::Node_ptr> nodes{root};

  while (nodes.size()) {
    auto node = nodes[0];
    nodes.erase(nodes.begin());

    if (node->get_type() == BDD::Node::BRANCH && !stop_on_branches) {
      auto branch_node = static_cast<const BDD::Branch *>(node.get());
      nodes.push_back(branch_node->get_on_true());
      nodes.push_back(branch_node->get_on_false());
    }

    else if (node->get_type() == BDD::Node::CALL) {
      auto call_node = static_cast<const BDD::Call *>(node.get());
      auto call = call_node->get_call();

      if (call.function_name == function_name) {
        functions.push_back(node);
      }

      nodes.push_back(node->get_next());
    }
  }

  return functions;
}

bool Module::is_parser_drop(BDD::Node_ptr root) const {
  assert(root);

  std::vector<BDD::Node_ptr> functions;
  std::vector<BDD::Node_ptr> nodes{root};

  while (nodes.size()) {
    auto node = nodes[0];
    nodes.erase(nodes.begin());

    switch (node->get_type()) {
    case BDD::Node::CALL: {
      auto call_node = BDD::cast_node<BDD::Call>(node);

      if (call_node->get_call().function_name != symbex::FN_RETURN_CHUNK) {
        return false;
      }

      nodes.push_back(node->get_next());
    } break;
    case BDD::Node::RETURN_PROCESS: {
      auto return_node = static_cast<const BDD::ReturnProcess *>(node.get());
      auto ret_value = return_node->get_return_operation();

      if (ret_value != BDD::ReturnProcess::Operation::DROP) {
        return false;
      }
    } break;
    default:
      return false;
    }
  }

  return true;
}

next_t get_allowed_coalescing_objs(std::vector<BDD::Node_ptr> index_allocators,
                                   obj_addr_t map_addr) {
  next_t candidates;

  for (auto allocator : index_allocators) {
    auto allocator_node = static_cast<const BDD::Call *>(allocator.get());
    auto allocator_call = allocator_node->get_call();

    assert(!allocator_call.args[symbex::FN_DCHAIN_ARG_OUT].out.isNull());
    auto allocated_index = allocator_call.args[symbex::FN_DCHAIN_ARG_OUT].out;

    /*
      We expect the coalescing candidates to be the same regardless
      of where in the BDD we are.
      In case there is some discrepancy, then it should be invalid.
      We thus consider only the intersection of all candidates.
    */

    auto new_candidates =
        get_next_maps_and_vectors_ops(allocator, map_addr, allocated_index);

    if (candidates.size() == 0) {
      candidates = new_candidates;
    } else {

      // If we can't find the current candidates in the new candidates' list,
      // then it is not true that we can coalesce them in every scenario of
      // the NF.

      candidates.intersect(new_candidates);
    }
  }

  return candidates;
}

Module::coalesced_data_t Module::get_coalescing_data(const ExecutionPlan &ep,
                                                     BDD::Node_ptr node) const {
  Module::coalesced_data_t coalesced_data;

  if (node->get_type() != BDD::Node::CALL) {
    return coalesced_data;
  }

  auto call_node = static_cast<const BDD::Call *>(node.get());
  auto call = call_node->get_call();

  if (call.function_name != symbex::FN_MAP_GET) {
    return coalesced_data;
  }

  assert(!call.args[symbex::FN_MAP_ARG_MAP].expr.isNull());
  assert(!call.args[symbex::FN_MAP_ARG_OUT].out.isNull());

  auto map = call.args[symbex::FN_MAP_ARG_MAP].expr;
  auto map_addr = kutil::expr_addr_to_obj_addr(map);
  auto index = call.args[symbex::FN_MAP_ARG_OUT].out;

  auto root = ep.get_bdd_root(node);
  assert(root);

  auto index_allocators =
      get_all_functions_after_node(root, symbex::FN_DCHAIN_ALLOCATE_NEW_INDEX);

  if (index_allocators.size() == 0) {
    return coalesced_data;
  }

  auto candidates = get_allowed_coalescing_objs(index_allocators, map_addr);

  if (candidates.size() == 0) {
    return coalesced_data;
  }

  auto incoming_maps_and_vectors =
      get_next_maps_and_vectors_ops(node, map_addr, index);

  incoming_maps_and_vectors.intersect(candidates);
  assert(incoming_maps_and_vectors.maps.size() == 0);

  if (incoming_maps_and_vectors.vectors.size() == 0) {
    return coalesced_data;
  }

  coalesced_data.can_coalesce = true;
  coalesced_data.map_get = node;
  coalesced_data.vector_borrows = incoming_maps_and_vectors.vectors;

  return coalesced_data;
}

} // namespace synapse
