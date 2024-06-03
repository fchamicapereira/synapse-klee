#include "util.h"

#include <bdd-visualizer.h>
#include <klee-util.h>

#include "targets/module.h"
#include "execution_plan/execution_plan.h"

#include <algorithm>

namespace synapse {

std::vector<modification_t> build_modifications(klee::ref<klee::Expr> before,
                                                klee::ref<klee::Expr> after) {
  std::vector<modification_t> _modifications;

  assert(before->getWidth() == after->getWidth());
  klee::Expr::Width size = before->getWidth();

  for (size_t b = 0; b < size; b += 8) {
    klee::ref<klee::Expr> before_byte =
        kutil::solver_toolbox.exprBuilder->Extract(before, b, klee::Expr::Int8);
    klee::ref<klee::Expr> after_byte =
        kutil::solver_toolbox.exprBuilder->Extract(after, b, klee::Expr::Int8);

    bool eq =
        kutil::solver_toolbox.are_exprs_always_equal(before_byte, after_byte);
    if (eq) {
      continue;
    }

    _modifications.emplace_back(b / 8, after_byte);
  }

  return _modifications;
}

// This is somewhat of a hack... We assume that checksum expressions will only
// be used to modify checksum fields of a packet, not other packet fields.
std::vector<modification_t> ignore_checksum_modifications(
    const std::vector<modification_t> &modifications) {
  std::vector<modification_t> filtered;

  for (const modification_t &mod : modifications) {
    klee::ref<klee::Expr> simplified = kutil::simplify(mod.expr);
    std::unordered_set<std::string> symbols = kutil::get_symbols(simplified);

    if (symbols.size() == 1 && simplified->getWidth() == 8) {
      const std::string &symbol = *symbols.begin();

      if (symbol.find("checksum") != std::string::npos) {
        continue;
      }
    }

    filtered.emplace_back(mod.byte, simplified);
  }

  return filtered;
}

bool query_contains_map_has_key(const bdd::Branch *node) {
  assert(!node->get_condition().isNull());
  klee::ref<klee::Expr> _condition = node->get_condition();

  kutil::SymbolRetriever retriever;
  retriever.visit(_condition);

  const std::unordered_set<std::string> &symbols =
      retriever.get_retrieved_strings();

  auto found_it = std::find_if(
      symbols.begin(), symbols.end(), [](const std::string &symbol) -> bool {
        return symbol.find("map_has_this_key") != std::string::npos;
      });

  if (found_it == symbols.end()) {
    return false;
  }

  return true;
}

klee::ref<klee::Expr> get_original_chunk(const EP *ep, const bdd::Node *node) {
  std::vector<const bdd::Node *> prev_borrows =
      get_prev_functions(ep, node, {"packet_borrow_next_chunk"});
  std::vector<const bdd::Node *> prev_returns =
      get_prev_functions(ep, node, {"packet_return_chunk"});

  assert(prev_borrows.size());
  assert(prev_borrows.size() > prev_returns.size());

  const bdd::Node *target = prev_borrows[prev_returns.size()];

  const bdd::Call *call_node = static_cast<const bdd::Call *>(target);
  const call_t &call = call_node->get_call();

  return call.extra_vars.at("the_chunk").second;
}

std::vector<const bdd::Node *>
get_prev_functions(const EP *ep, const bdd::Node *node,
                   const std::vector<std::string> &fnames) {
  std::vector<const bdd::Node *> prev_functions;

  TargetType target = ep->get_current_platform();
  const bdd::nodes_t &roots = ep->get_target_roots(target);

  if (!node) {
    return prev_functions;
  }

  while ((node = node->get_prev())) {
    if (node->get_type() == bdd::NodeType::CALL) {
      const bdd::Call *call_node = static_cast<const bdd::Call *>(node);
      const call_t &call = call_node->get_call();
      const std::string &fname = call.function_name;

      auto found_it = std::find(fnames.begin(), fnames.end(), fname);
      if (found_it != fnames.end()) {
        prev_functions.push_back(node);
      }
    }

    if (roots.find(node->get_id()) != roots.end()) {
      break;
    }
  }

  return prev_functions;
}

std::vector<const bdd::Node *>
get_all_functions_after_node(const bdd::Node *root,
                             const std::vector<std::string> &wanted,
                             bool stop_on_branches) {
  std::vector<const bdd::Node *> functions;
  std::vector<const bdd::Node *> nodes{root};

  while (nodes.size()) {
    const bdd::Node *node = nodes[0];
    nodes.erase(nodes.begin());

    if (node->get_type() == bdd::NodeType::BRANCH && !stop_on_branches) {
      const bdd::Branch *branch_node = static_cast<const bdd::Branch *>(node);
      nodes.push_back(branch_node->get_on_true());
      nodes.push_back(branch_node->get_on_false());
    }

    else if (node->get_type() == bdd::NodeType::CALL) {
      const bdd::Call *call_node = static_cast<const bdd::Call *>(node);
      const call_t &call = call_node->get_call();

      auto found_it =
          std::find(wanted.begin(), wanted.end(), call.function_name);

      if (found_it != wanted.end()) {
        functions.push_back(node);
      }

      nodes.push_back(node->get_next());
    }
  }

  return functions;
}

bool is_parser_drop(const bdd::Node *root) {
  bool found_drop = false;

  root->visit_nodes([&found_drop](const bdd::Node *node) {
    if (node->get_type() == bdd::NodeType::BRANCH) {
      return bdd::NodeVisitAction::STOP;
    }

    if (node->get_type() != bdd::NodeType::ROUTE) {
      return bdd::NodeVisitAction::VISIT_CHILDREN;
    }

    const bdd::Route *route_node = static_cast<const bdd::Route *>(node);
    bdd::RouteOperation op = route_node->get_operation();

    found_drop = (op == bdd::RouteOperation::DROP);
    return bdd::NodeVisitAction::STOP;
  });

  return found_drop;
}

std::vector<const Module *>
get_prev_modules(const EP *ep, const std::vector<ModuleType> &targets) {
  std::vector<const Module *> modules;

  const EPLeaf *leaf = ep->get_active_leaf();
  const EPNode *ep_node = leaf->node;

  while (ep_node) {
    const EPNode *current = ep_node;
    ep_node = current->get_prev();

    const Module *module = current->get_module();
    ModuleType type = module->get_type();

    auto found_it = std::find(targets.begin(), targets.end(), type);
    if (found_it != targets.end()) {
      modules.push_back(module);
    }
  }

  return modules;
}

bool is_expr_only_packet_dependent(klee::ref<klee::Expr> expr) {
  kutil::SymbolRetriever retriever;
  retriever.visit(expr);

  const std::unordered_set<std::string> &symbols =
      retriever.get_retrieved_strings();

  std::vector<std::string> allowed_symbols = {
      "pkt_len",
      "packet_chunks",
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

static bool is_incrementing_op(klee::ref<klee::Expr> before,
                               klee::ref<klee::Expr> after) {
  if (after->getKind() != klee::Expr::Add) {
    return false;
  }

  klee::ref<klee::Expr> lhs = after->getKid(0);
  klee::ref<klee::Expr> rhs = after->getKid(1);

  bool lhs_is_const = lhs->getKind() == klee::Expr::Constant;
  bool rhs_is_const = rhs->getKind() == klee::Expr::Constant;

  if (!lhs_is_const && !rhs_is_const) {
    return false;
  }

  klee::ref<klee::Expr> const_expr = lhs_is_const ? lhs : rhs;
  klee::ref<klee::Expr> not_const_expr = lhs_is_const ? rhs : lhs;

  // We only support increment of one, for now...
  assert(kutil::solver_toolbox.value_from_expr(const_expr) == 1);

  return kutil::solver_toolbox.are_exprs_always_equal(not_const_expr, before);
}

static std::optional<uint64_t>
get_max_value(klee::ref<klee::Expr> original_value,
              klee::ref<klee::Expr> condition) {
  std::optional<uint64_t> max_value;

  std::optional<std::string> original_symbol =
      kutil::get_symbol(original_value);
  assert(original_symbol.has_value());

  std::optional<std::string> symbol = kutil::get_symbol(condition);

  if (!symbol.has_value()) {
    return max_value;
  }

  // We are looking for expression that look like this:
  // !(65536 <= vector_data_reset)
  // We should be more careful with this and be compatible with more
  // expressions.

  while (condition->getKind() == klee::Expr::Not) {
    condition = condition->getKid(0);
  }

  if (condition->getKind() == klee::Expr::Eq) {
    klee::ref<klee::Expr> lhs = condition->getKid(0);
    klee::ref<klee::Expr> rhs = condition->getKid(1);

    if (lhs->getKind() != klee::Expr::Constant) {
      return max_value;
    }

    uint64_t const_value = kutil::solver_toolbox.value_from_expr(lhs);

    if (const_value != 0) {
      return max_value;
    }

    condition = rhs;
  }

  if (condition->getKind() != klee::Expr::Ule) {
    return max_value;
  }

  klee::ref<klee::Expr> lhs = condition->getKid(0);
  klee::ref<klee::Expr> rhs = condition->getKid(1);

  bool lhs_is_const = lhs->getKind() == klee::Expr::Constant;
  bool rhs_is_const = rhs->getKind() == klee::Expr::Constant;

  if (!lhs_is_const && !rhs_is_const) {
    return max_value;
  }

  klee::ref<klee::Expr> const_expr = lhs_is_const ? lhs : rhs;
  klee::ref<klee::Expr> not_const_expr = lhs_is_const ? rhs : lhs;

  if (!kutil::solver_toolbox.are_exprs_always_equal(not_const_expr,
                                                    original_value)) {
    return max_value;
  }

  return kutil::solver_toolbox.value_from_expr(const_expr);
}

static bool is_counter_inc_op(const bdd::Node *vector_borrow,
                              std::optional<uint64_t> &max_value) {
  const bdd::Node *branch = vector_borrow->get_next();
  const bdd::Branch *branch_node = static_cast<const bdd::Branch *>(branch);

  if (!branch_node) {
    return false;
  }

  klee::ref<klee::Expr> condition = branch_node->get_condition();

  const bdd::Node *on_true = branch_node->get_on_true();
  const bdd::Node *on_false = branch_node->get_on_false();

  const bdd::Call *borrow_node = static_cast<const bdd::Call *>(vector_borrow);
  const bdd::Call *on_true_node = static_cast<const bdd::Call *>(on_true);
  const bdd::Call *on_false_node = static_cast<const bdd::Call *>(on_false);

  if (!on_true_node || !on_false_node) {
    return false;
  }

  const call_t &borrow_call = borrow_node->get_call();
  const call_t &on_true_call = on_true_node->get_call();
  const call_t &on_false_call = on_false_node->get_call();

  if (on_true_call.function_name != "vector_return" ||
      on_false_call.function_name != "vector_return") {
    return false;
  }

  klee::ref<klee::Expr> borrow_vector = borrow_call.args.at("vector").expr;
  klee::ref<klee::Expr> on_true_vector = on_true_call.args.at("vector").expr;
  klee::ref<klee::Expr> on_false_vector = on_false_call.args.at("vector").expr;

  addr_t borrow_vector_addr = kutil::expr_addr_to_obj_addr(borrow_vector);
  addr_t on_true_vector_addr = kutil::expr_addr_to_obj_addr(on_true_vector);
  addr_t on_false_vector_addr = kutil::expr_addr_to_obj_addr(on_false_vector);

  if (borrow_vector_addr != on_true_vector_addr ||
      borrow_vector_addr != on_false_vector_addr) {
    return false;
  }

  klee::ref<klee::Expr> borrow_value =
      borrow_call.extra_vars.at("borrowed_cell").second;
  klee::ref<klee::Expr> on_true_value = on_true_call.args.at("value").in;
  klee::ref<klee::Expr> on_false_value = on_false_call.args.at("value").in;

  bool on_true_inc_op = is_incrementing_op(borrow_value, on_true_value);

  if (!on_true_inc_op) {
    return false;
  }

  bool on_false_eq = kutil::solver_toolbox.are_exprs_always_equal(
      borrow_value, on_false_value);

  if (!on_false_eq) {
    return false;
  }

  std::optional<uint64_t> local_max_value =
      get_max_value(borrow_value, condition);
  assert(local_max_value.has_value() && "Expecting a max value for counter.");

  if (!max_value.has_value()) {
    max_value = local_max_value;
  } else if (*max_value != *local_max_value) {
    return false;
  }

  return true;
}

static bool is_counter_read_op(const bdd::Node *vector_borrow) {
  const bdd::Node *vector_return = vector_borrow->get_next();

  const bdd::Call *borrow_node = static_cast<const bdd::Call *>(vector_borrow);
  const bdd::Call *return_node = static_cast<const bdd::Call *>(vector_return);

  if (!return_node) {
    return false;
  }

  const call_t &borrow_call = borrow_node->get_call();
  const call_t &return_call = return_node->get_call();

  if (return_call.function_name != "vector_return") {
    return false;
  }

  klee::ref<klee::Expr> borrow_vector = borrow_call.args.at("vector").expr;
  klee::ref<klee::Expr> return_vector = return_call.args.at("vector").expr;

  addr_t borrow_vector_addr = kutil::expr_addr_to_obj_addr(borrow_vector);
  addr_t return_vector_addr = kutil::expr_addr_to_obj_addr(return_vector);

  if (borrow_vector_addr != return_vector_addr) {
    return false;
  }

  klee::ref<klee::Expr> borrow_value =
      borrow_call.extra_vars.at("borrowed_cell").second;
  klee::ref<klee::Expr> return_value = return_call.args.at("value").in;

  bool equal_values =
      kutil::solver_toolbox.are_exprs_always_equal(borrow_value, return_value);

  return equal_values;
}

counter_data_t is_counter(const EP *ep, addr_t obj) {
  counter_data_t data;

  const bdd::BDD *bdd = ep->get_bdd();
  bdd::vector_config_t cfg = bdd::get_vector_config(*bdd, obj);

  if (cfg.elem_size > 64 || cfg.capacity != 1) {
    return data;
  }

  const bdd::Node *root = bdd->get_root();
  std::vector<const bdd::Node *> vector_borrows =
      get_all_functions_after_node(root, {"vector_borrow"});

  for (const bdd::Node *vector_borrow : vector_borrows) {
    const bdd::Call *call_node = static_cast<const bdd::Call *>(vector_borrow);
    const call_t &call = call_node->get_call();

    auto _vector = call.args.at("vector").expr;
    auto _vector_addr = kutil::expr_addr_to_obj_addr(_vector);

    if (_vector_addr != obj) {
      continue;
    }

    if (is_counter_read_op(vector_borrow)) {
      data.reads.push_back(vector_borrow);
      continue;
    }

    if (is_counter_inc_op(vector_borrow, data.max_value)) {
      data.writes.push_back(vector_borrow);
      continue;
    }

    return data;
  }

  data.valid = true;
  return data;
}

klee::ref<klee::Expr> get_original_vector_value(const EP *ep,
                                                const bdd::Node *node,
                                                addr_t target_addr) {
  const bdd::Node *source;
  return get_original_vector_value(ep, node, target_addr, source);
}

klee::ref<klee::Expr> get_original_vector_value(const EP *ep,
                                                const bdd::Node *node,
                                                addr_t target_addr,
                                                const bdd::Node *&source) {
  std::vector<const bdd::Node *> all_prev_vector_borrow =
      get_prev_functions(ep, node, {"vector_borrow"});

  klee::ref<klee::Expr> borrowed_cell;

  for (const bdd::Node *prev_vector_borrow : all_prev_vector_borrow) {
    const bdd::Call *call_node =
        static_cast<const bdd::Call *>(prev_vector_borrow);
    const call_t &call = call_node->get_call();

    klee::ref<klee::Expr> _vector = call.args.at("vector").expr;
    klee::ref<klee::Expr> _borrowed_cell =
        call.extra_vars.at("borrowed_cell").second;

    addr_t _vector_addr = kutil::expr_addr_to_obj_addr(_vector);

    if (_vector_addr != target_addr) {
      continue;
    }

    source = prev_vector_borrow;
    borrowed_cell = _borrowed_cell;
    break;
  }

  assert(!borrowed_cell.isNull() &&
         "Expecting a previous vector borrow but not found.");
  return borrowed_cell;
}

const bdd::Node *get_future_vector_return(const bdd::Node *root,
                                          addr_t target_addr) {
  std::vector<const bdd::Node *> vector_returns =
      get_all_functions_after_node(root, {"vector_return"});

  for (const bdd::Node *vector_return : vector_returns) {
    assert(vector_return->get_type() == bdd::NodeType::CALL);
    const bdd::Call *call_node = static_cast<const bdd::Call *>(vector_return);
    const call_t &call = call_node->get_call();

    klee::ref<klee::Expr> vector_addr_expr = call.args.at("vector").expr;
    addr_t obj = kutil::expr_addr_to_obj_addr(vector_addr_expr);

    if (obj != target_addr) {
      continue;
    }

    return vector_return;
  }

  return nullptr;
}

klee::ref<klee::Expr> get_expr_from_addr(const EP *ep, addr_t addr) {
  const bdd::BDD *bdd = ep->get_bdd();
  const bdd::Node *root = bdd->get_root();
  std::vector<const bdd::Node *> nodes =
      get_all_functions_after_node(root, {"map_get"});

  for (const bdd::Node *node : nodes) {
    const bdd::Call *call_node = static_cast<const bdd::Call *>(node);
    const call_t &call = call_node->get_call();

    klee::ref<klee::Expr> key_addr = call.args.at("key").expr;
    klee::ref<klee::Expr> key = call.args.at("key").in;
    addr_t key_addr_value = kutil::expr_addr_to_obj_addr(key_addr);

    if (key_addr_value != addr) {
      continue;
    }

    return key;
  }

  return nullptr;
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
  objs_t maps;
  objs_t vectors;
  objs_t dchains;

  int size() const { return maps.size() + vectors.size(); }

  void intersect(const next_t &other) {
    auto intersector = [](objs_t &a, const objs_t &b) {
      for (auto it = a.begin(); it != a.end();) {
        if (b.find(*it) == b.end()) {
          it = a.erase(it);
        } else {
          it++;
        }
      }
    };

    intersector(maps, other.maps);
    intersector(vectors, other.vectors);
    intersector(dchains, other.dchains);
  }

  bool has_obj(addr_t obj) const {
    if (maps.find(obj) != maps.end()) {
      return true;
    }

    if (vectors.find(obj) != vectors.end()) {
      return true;
    }

    if (dchains.find(obj) != dchains.end()) {
      return true;
    }

    return false;
  }
};

static next_t get_next_maps_and_vectors(const bdd::Node *root,
                                        klee::ref<klee::Expr> index) {
  next_t candidates;

  root->visit_nodes([&candidates, index](const bdd::Node *node) {
    if (node->get_type() != bdd::NodeType::CALL) {
      return bdd::NodeVisitAction::VISIT_CHILDREN;
    }

    const bdd::Call *call_node = static_cast<const bdd::Call *>(node);
    const call_t &call = call_node->get_call();

    if (call.function_name == "map_put") {
      klee::ref<klee::Expr> map = call.args.at("map").expr;
      klee::ref<klee::Expr> value = call.args.at("value").expr;

      addr_t map_addr = kutil::expr_addr_to_obj_addr(map);
      bool same_index =
          kutil::solver_toolbox.are_exprs_always_equal(index, value);

      if (same_index) {
        candidates.maps.insert(map_addr);
      }
    }

    else if (call.function_name == "vector_borrow") {
      klee::ref<klee::Expr> vector = call.args.at("vector").expr;
      klee::ref<klee::Expr> value = call.args.at("index").expr;

      addr_t vector_addr = kutil::expr_addr_to_obj_addr(vector);
      bool same_index =
          kutil::solver_toolbox.are_exprs_always_equal(index, value);

      if (same_index) {
        candidates.vectors.insert(vector_addr);
      }
    }

    return bdd::NodeVisitAction::VISIT_CHILDREN;
  });

  return candidates;
}

static next_t
get_allowed_coalescing_objs(std::vector<const bdd::Node *> index_allocators,
                            addr_t obj) {
  next_t candidates;

  for (const bdd::Node *allocator : index_allocators) {
    const bdd::Call *allocator_node = static_cast<const bdd::Call *>(allocator);
    const call_t &allocator_call = allocator_node->get_call();

    klee::ref<klee::Expr> allocated_index =
        allocator_call.args.at("index_out").out;
    klee::ref<klee::Expr> dchain = allocator_call.args.at("chain").expr;
    addr_t dchain_addr = kutil::expr_addr_to_obj_addr(dchain);

    // We expect the coalescing candidates to be the same regardless of
    // where in the BDD we are. In case there is some discrepancy, then it
    // should be invalid. We thus consider only the intersection of all
    // candidates.

    next_t new_candidates =
        get_next_maps_and_vectors(allocator, allocated_index);
    new_candidates.dchains.insert(dchain_addr);

    if (!new_candidates.has_obj(obj)) {
      continue;
    }

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

std::optional<map_coalescing_data_t>
get_map_coalescing_data(const bdd::BDD *bdd, addr_t obj) {
  const bdd::Node *root = bdd->get_root();

  std::vector<const bdd::Node *> index_allocators =
      get_all_functions_after_node(root, {"dchain_allocate_new_index"});

  if (index_allocators.size() == 0) {
    return std::nullopt;
  }

  next_t candidates = get_allowed_coalescing_objs(index_allocators, obj);

  if (candidates.size() == 0) {
    return std::nullopt;
  }

  assert(candidates.maps.size() == 1);
  assert(candidates.dchains.size() == 1);

  map_coalescing_data_t data;
  data.map = *candidates.maps.begin();
  data.dchain = *candidates.dchains.begin();
  data.vectors = candidates.vectors;

  return data;
}

bool is_parser_condition(const bdd::Node *node) {
  if (node->get_type() != bdd::NodeType::BRANCH) {
    return false;
  }

  const bdd::Branch *branch_node = static_cast<const bdd::Branch *>(node);

  std::vector<const bdd::Node *> future_borrows =
      get_all_functions_after_node(node, {"packet_borrow_next_chunk"});

  if (future_borrows.size() == 0) {
    return false;
  }

  klee::ref<klee::Expr> condition = branch_node->get_condition();
  bool only_looks_at_packet = is_expr_only_packet_dependent(condition);

  return only_looks_at_packet;
}

klee::ref<klee::Expr> get_chunk_from_borrow(const bdd::Node *node) {
  if (node->get_type() != bdd::NodeType::CALL) {
    return nullptr;
  }

  const bdd::Call *call_node = static_cast<const bdd::Call *>(node);
  const call_t &call = call_node->get_call();

  if (call.function_name != "packet_borrow_next_chunk") {
    return nullptr;
  }

  return call.extra_vars.at("the_chunk").second;
}

bool borrow_has_var_len(const bdd::Node *node) {
  if (node->get_type() != bdd::NodeType::CALL) {
    return false;
  }

  const bdd::Call *call_node = static_cast<const bdd::Call *>(node);
  const call_t &call = call_node->get_call();

  if (call.function_name != "packet_borrow_next_chunk") {
    return false;
  }

  klee::ref<klee::Expr> length = call.args.at("length").expr;
  return length->getKind() != klee::Expr::Kind::Constant;
}

symbols_t get_prev_symbols(const bdd::Node *node,
                           const bdd::nodes_t &stop_nodes) {
  symbols_t symbols;

  while (node) {
    if (stop_nodes.find(node->get_id()) != stop_nodes.end()) {
      break;
    }

    if (node->get_type() == bdd::NodeType::CALL) {
      const bdd::Call *call_node = static_cast<const bdd::Call *>(node);
      symbols_t local_symbols = call_node->get_locally_generated_symbols();
      symbols.insert(local_symbols.begin(), local_symbols.end());
    }

    node = node->get_prev();
  }

  return symbols;
}

bool is_vector_return_without_modifications(const EP *ep,
                                            const bdd::Node *node) {
  if (node->get_type() != bdd::NodeType::CALL) {
    return false;
  }

  const bdd::Call *call_node = static_cast<const bdd::Call *>(node);
  const call_t &call = call_node->get_call();

  if (call.function_name != "vector_return") {
    return false;
  }

  klee::ref<klee::Expr> vector_addr_expr = call.args.at("vector").expr;
  klee::ref<klee::Expr> value = call.args.at("value").in;

  addr_t vector_addr = kutil::expr_addr_to_obj_addr(vector_addr_expr);

  klee::ref<klee::Expr> original_value =
      get_original_vector_value(ep, call_node, vector_addr);
  std::vector<modification_t> changes =
      build_modifications(original_value, value);

  return changes.empty();
}

} // namespace synapse