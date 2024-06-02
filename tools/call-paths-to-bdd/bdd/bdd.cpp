#include <unordered_map>
#include <set>
#include <iostream>
#include <optional>

#include "bdd-io.h"
#include "bdd.h"

#include "call-paths-groups.h"
#include "klee-util.h"
#include "visitor.h"

#include "nodes/branch.h"
#include "nodes/call.h"
#include "nodes/route.h"

namespace bdd {

static std::vector<std::string> ignored_functions{
    "start_time",
    "current_time",
    "loop_invariant_consume",
    "loop_invariant_produce",
    "packet_receive",
    "packet_state_total_length",
    "packet_get_unread_length",
    "vector_reset",
};

static std::vector<std::string> init_functions{
    "map_allocate",    "vector_allocate", "dchain_allocate",
    "sketch_allocate", "cht_fill_cht",
};

static std::vector<std::string> symbols_in_skippable_conditions{
    "received_a_packet",           "loop_termination",
    "map_allocation_succeeded",    "vector_alloc_success",
    "is_dchain_allocated",         "cht_fill_cht_successful",
    "sketch_allocation_succeeded",
};

static std::vector<std::string> rounting_functions{
    "packet_send",
    "packet_free",
    "packet_broadcast",
};

static std::unordered_set<std::string> bdd_symbols{
    "DEVICE",
    "pkt_len",
    "next_time",
};

static std::unordered_map<std::string, std::unordered_set<std::string>>
    symbols_from_call = {
        {
            "rte_lcore_count",
            {"lcores"},
        },
        {
            "rte_ether_addr_hash",
            {"rte_ether_addr_hash"},
        },
        {
            "nf_set_rte_ipv4_udptcp_checksum",
            {"checksum"},
        },
        {
            "current_time",
            {"next_time"},
        },
        {
            "packet_receive",
            {"DEVICE", "pkt_len"},
        },
        {
            "expire_items_single_map",
            {"number_of_freed_flows"},
        },
        {
            "expire_items_single_map_iteratively",
            {"number_of_freed_flows"},
        },
        {
            "map_get",
            {"map_has_this_key", "allocated_index"},
        },
        {
            "dchain_is_index_allocated",
            {"dchain_is_index_allocated"},
        },
        {
            "dchain_allocate_new_index",
            {"out_of_space", "new_index"},
        },
        {
            "vector_borrow",
            {"vector_data_reset"},
        },
        {
            "cht_find_preferred_available_backend",
            {"chosen_backend", "prefered_backend_found"},
        },
        {
            "sketch_fetch",
            {"overflow"},
        },
        {
            "sketch_touch_buckets",
            {"success"},
        },
        {
            "hash_obj",
            {"hash"},
        },
};

typedef symbols_t (*SymbolsExtractor)(const call_t &call,
                                      const symbols_t &all_symbols);

static symbols_t packet_chunks_symbol_extractor(const call_t &call,
                                                const symbols_t &all_symbols) {
  assert(call.function_name == "packet_borrow_next_chunk");

  const extra_var_t &extra_var = call.extra_vars.at("the_chunk");
  klee::ref<klee::Expr> packet_chunk = extra_var.second;

  symbol_t symbol;
  bool found = get_symbol(all_symbols, "packet_chunks", symbol);
  assert(found && "Symbol not found");

  symbol.expr = packet_chunk;
  return {symbol};
}

static std::unordered_map<std::string, SymbolsExtractor>
    special_symbols_extractors = {
        {
            "packet_borrow_next_chunk",
            packet_chunks_symbol_extractor,
        },
};

void BDD::visit(BDDVisitor &visitor) const { visitor.visit(*this); }

const Node *BDD::get_node_by_id(node_id_t _id) const {
  return root ? root->get_node_by_id(_id) : nullptr;
}

Node *BDD::get_mutable_node_by_id(node_id_t _id) {
  return root ? root->get_mutable_node_by_id(_id) : nullptr;
}

static bool is_bdd_symbol(const std::string &symbol) {
  return bdd_symbols.find(symbol) != bdd_symbols.end();
}

static bool is_init_function(const call_t &call) {
  return std::find(init_functions.begin(), init_functions.end(),
                   call.function_name) != init_functions.end();
}

static bool is_skip_function(const call_t &call) {
  return std::find(ignored_functions.begin(), ignored_functions.end(),
                   call.function_name) != ignored_functions.end();
}

static bool is_routing_function(const call_t &call) {
  return std::find(rounting_functions.begin(), rounting_functions.end(),
                   call.function_name) != rounting_functions.end();
}

static bool is_skip_condition(klee::ref<klee::Expr> condition) {
  kutil::SymbolRetriever retriever;
  retriever.visit(condition);

  const std::unordered_set<std::string> &symbols =
      retriever.get_retrieved_strings();

  for (const std::string &symbol : symbols) {
    auto base_symbol_comparator = [symbol](const std::string &s) {
      return symbol.find(s) != std::string::npos;
    };

    auto found_it = std::find_if(symbols_in_skippable_conditions.begin(),
                                 symbols_in_skippable_conditions.end(),
                                 base_symbol_comparator);

    if (found_it != symbols_in_skippable_conditions.end())
      return true;
  }

  return false;
}

static Route *route_node_from_call(const call_t &call,
                                   const klee::ConstraintManager &constraints,
                                   node_id_t id) {
  assert(is_routing_function(call));

  if (call.function_name == "packet_free") {
    return new Route(id, constraints, bdd::RouteOperation::DROP);
  }

  if (call.function_name == "packet_broadcast") {
    return new Route(id, constraints, bdd::RouteOperation::BCAST);
  }

  assert(call.function_name == "packet_send");

  klee::ref<klee::Expr> dst_device = call.args.at("dst_device").expr;
  assert(!dst_device.isNull());

  int value = kutil::solver_toolbox.value_from_expr(dst_device);
  return new Route(id, constraints, bdd::RouteOperation::FWD, value);
}

static call_t
get_successful_call(const std::vector<call_path_t *> &call_paths) {
  assert(call_paths.size());

  for (call_path_t *cp : call_paths) {
    assert(cp->calls.size());
    const call_t &call = cp->calls[0];

    if (call.ret.isNull())
      return call;

    klee::ref<klee::Expr> zero =
        kutil::solver_toolbox.exprBuilder->Constant(0, call.ret->getWidth());
    klee::ref<klee::Expr> eq_zero =
        kutil::solver_toolbox.exprBuilder->Eq(call.ret, zero);
    bool is_ret_success = kutil::solver_toolbox.is_expr_always_false(eq_zero);

    if (is_ret_success)
      return call;
  }

  // No function with successful return.
  std::cerr << "No function with successful return!\n";
  return call_paths[0]->calls[0];
}

static klee::ref<klee::Expr>
simplify_constraint(klee::ref<klee::Expr> constraint) {
  assert(!constraint.isNull());

  klee::ref<klee::Expr> simplified = kutil::simplify(constraint);

  if (kutil::is_bool(simplified))
    return simplified;

  klee::Expr::Width width = simplified->getWidth();
  klee::ref<klee::Expr> zero =
      kutil::solver_toolbox.exprBuilder->Constant(0, width);
  klee::ref<klee::Expr> is_not_zero =
      kutil::solver_toolbox.exprBuilder->Ne(simplified, zero);

  return is_not_zero;
}

static klee::ref<klee::Expr>
negate_and_simplify_constraint(klee::ref<klee::Expr> constraint) {
  assert(!constraint.isNull());

  klee::ref<klee::Expr> simplified = kutil::simplify(constraint);

  if (kutil::is_bool(simplified))
    return kutil::solver_toolbox.exprBuilder->Not(simplified);

  klee::Expr::Width width = simplified->getWidth();
  klee::ref<klee::Expr> zero =
      kutil::solver_toolbox.exprBuilder->Constant(0, width);
  klee::ref<klee::Expr> is_zero =
      kutil::solver_toolbox.exprBuilder->Eq(simplified, zero);

  return is_zero;
}

static std::optional<symbol_t> get_generated_symbol(
    const symbols_t &symbols, const std::string &base_symbol,
    std::unordered_map<std::string, size_t> &base_symbols_generated) {
  std::vector<symbol_t> filtered;

  for (const symbol_t &symbol : symbols) {
    if (symbol.base == base_symbol)
      filtered.push_back(symbol);
  }

  auto symbol_cmp = [base_symbol](const symbol_t &a, const symbol_t &b) {
    size_t base_a_pos = a.array->name.find(base_symbol);
    size_t base_b_pos = b.array->name.find(base_symbol);

    std::string a_suffix =
        a.array->name.substr(base_a_pos + base_symbol.size());
    std::string b_suffix =
        b.array->name.substr(base_b_pos + base_symbol.size());

    if (a_suffix.size() == 0)
      return true;

    if (b_suffix.size() == 0)
      return false;

    assert(a_suffix[0] == '_');
    assert(b_suffix[0] == '_');

    int a_suffix_num = std::stoi(a_suffix.substr(1));
    int b_suffix_num = std::stoi(b_suffix.substr(1));

    return a_suffix_num < b_suffix_num;
  };

  std::sort(filtered.begin(), filtered.end(), symbol_cmp);

  size_t total_base_symbols_generated = 0;

  if (base_symbols_generated.find(base_symbol) !=
      base_symbols_generated.end()) {
    total_base_symbols_generated = base_symbols_generated[base_symbol];
  } else {
    base_symbols_generated[base_symbol] = 0;
  }

  if (total_base_symbols_generated >= filtered.size()) {
    // Not enough symbols for translation
    return std::nullopt;
  }

  return filtered[total_base_symbols_generated];
}

static symbols_t get_generated_symbols(
    const call_t &call,
    std::unordered_map<std::string, size_t> &base_symbols_generated,
    const symbols_t &symbols) {
  symbols_t generated_symbols;

  if (special_symbols_extractors.find(call.function_name) !=
      special_symbols_extractors.end()) {
    return special_symbols_extractors[call.function_name](call, symbols);
  }

  auto found_it = symbols_from_call.find(call.function_name);
  if (found_it == symbols_from_call.end())
    return generated_symbols;

  const std::unordered_set<std::string> &call_symbols = found_it->second;

  for (const std::string &base_symbol : call_symbols) {
    std::optional<symbol_t> new_symbol =
        get_generated_symbol(symbols, base_symbol, base_symbols_generated);

    if (new_symbol) {
      generated_symbols.insert(*new_symbol);
      base_symbols_generated[base_symbol]++;
    }
  }

  return generated_symbols;
}

static void pop_call_paths(call_paths_t &call_paths) {
  for (call_path_t *cp : call_paths.cps) {
    assert(cp->calls.size());
    cp->calls.erase(cp->calls.begin());
  }
}

static void build_bdd_symbols(const symbols_t &symbols,
                              symbols_t &bdd_symbols) {
  for (const symbol_t &symbol : symbols) {
    if (is_bdd_symbol(symbol.base)) {
      bdd_symbols.insert(symbol);
    }
  }
}

static Node *bdd_from_call_paths(
    call_paths_t call_paths, NodeManager &manager, std::vector<call_t> &init,
    node_id_t &id, symbols_t &bdd_symbols,
    klee::ConstraintManager constraints = klee::ConstraintManager(),
    std::unordered_map<std::string, size_t> base_symbols_generated =
        std::unordered_map<std::string, size_t>()) {
  Node *root = nullptr;
  Node *leaf = nullptr;

  if (call_paths.cps[0]->calls.size() == 0)
    return root;

  symbols_t symbols = call_paths.get_symbols();

  while (call_paths.cps.size()) {
    CallPathsGroup group(call_paths);

    const call_paths_t &on_true = group.get_on_true();
    const call_paths_t &on_false = group.get_on_false();

    if (on_true.cps[0]->calls.size() == 0) {
      break;
    }

    if (on_true.cps.size() == call_paths.cps.size()) {
      assert(on_false.cps.size() == 0);

      call_t call = get_successful_call(call_paths.cps);

      symbols_t generated_symbols =
          get_generated_symbols(call, base_symbols_generated, symbols);

      build_bdd_symbols(generated_symbols, bdd_symbols);

      std::cerr << "\n";
      std::cerr << "==================================\n";
      std::cerr << "Call: " << call << "\n";
      std::cerr << "Call paths:\n";
      for (const call_path_t *cp : call_paths.cps)
        std::cerr << "  " << cp->file_name << "\n";
      std::cerr << "Generated symbols (" << generated_symbols.size() << "):\n";
      for (const symbol_t &symbol : generated_symbols)
        std::cerr << "  " << kutil::expr_to_string(symbol.expr, true) << "\n";
      std::cerr << "Constraints (" << constraints.size() << "):\n";
      for (const klee::ref<klee::Expr> &constraint : constraints)
        std::cerr << "  " << kutil::expr_to_string(constraint, true) << "\n";
      std::cerr << "==================================\n";

      if (is_init_function(call)) {
        init.push_back(call);
      } else if (!is_skip_function(call)) {
        Node *node;

        if (is_routing_function(call)) {
          node = route_node_from_call(call, constraints, id);
        } else {
          node = new Call(id, constraints, call, generated_symbols);
        }

        manager.add_node(node);
        id++;

        if (root == nullptr) {
          root = node;
          leaf = node;
        } else {
          leaf->set_next(node);
          node->set_prev(leaf);
          leaf = node;
        }
      }

      pop_call_paths(call_paths);
    } else {
      klee::ref<klee::Expr> discriminating_constraint =
          group.get_discriminating_constraint();

      klee::ref<klee::Expr> condition =
          simplify_constraint(discriminating_constraint);
      klee::ref<klee::Expr> not_condition =
          negate_and_simplify_constraint(discriminating_constraint);

      klee::ConstraintManager on_true_constraints = constraints;
      klee::ConstraintManager on_false_constraints = constraints;

      on_true_constraints.addConstraint(condition);
      on_false_constraints.addConstraint(not_condition);

      std::cerr << "\n";
      std::cerr << "==================================\n";
      std::cerr << "Condition: " << kutil::expr_to_string(condition, true)
                << "\n";
      std::cerr << "On true call paths:\n";
      for (const call_path_t *cp : on_true.cps)
        std::cerr << "  " << cp->file_name << "\n";
      std::cerr << "On False call paths:\n";
      for (const call_path_t *cp : on_false.cps)
        std::cerr << "  " << cp->file_name << "\n";
      std::cerr << "==================================\n";

      if (is_skip_condition(condition)) {
        // Assumes the right path is the one with the most call paths (or the on
        // true path, somehwat arbitrarily...)
        if (on_true.cps.size() >= on_false.cps.size()) {
          call_paths = on_true;
        } else {
          call_paths = on_false;
        }
        continue;
      }

      Branch *node = new Branch(id, constraints, condition);
      id++;
      manager.add_node(node);

      Node *on_true_root =
          bdd_from_call_paths(on_true, manager, init, id, bdd_symbols,
                              on_true_constraints, base_symbols_generated);
      Node *on_false_root =
          bdd_from_call_paths(on_false, manager, init, id, bdd_symbols,
                              on_false_constraints, base_symbols_generated);

      assert(on_true_root && on_false_root);

      node->set_on_true(on_true_root);
      node->set_on_false(on_false_root);

      on_true_root->set_prev(node);
      on_false_root->set_prev(node);

      if (root == nullptr)
        return node;

      leaf->set_next(node);
      node->set_prev(leaf);

      return root;
    }
  }

  return root;
}

symbols_t BDD::get_generated_symbols(const Node *node) const {
  symbols_t symbols{device, packet_len, time};

  while (node) {
    if (node->get_type() == NodeType::CALL) {
      const Call *call_node = static_cast<const Call *>(node);
      symbols_t more_symbols = call_node->get_locally_generated_symbols();
      symbols.insert(more_symbols.begin(), more_symbols.end());
    }

    node = node->get_prev();
  }

  return symbols;
}

BDD::BDD(const call_paths_t &call_paths) : id(0) {
  symbols_t bdd_symbols;
  root = bdd_from_call_paths(call_paths, manager, init, id, bdd_symbols);

  for (const symbol_t &symbol : bdd_symbols) {
    if (symbol.base == "DEVICE") {
      device = symbol;
    } else if (symbol.base == "pkt_len") {
      packet_len = symbol;
    } else if (symbol.base == "next_time") {
      time = symbol;
    } else {
      assert(false && "Unknown BDD symbol");
    }
  }

  // Some NFs don't care about the device, so it doesn't show up on the call
  // paths.
  if (device.expr.isNull()) {
    device.expr = kutil::solver_toolbox.create_new_symbol("DEVICE", 32);
  }
}

BDD::BDD(const std::string &file_path) : id(0) { deserialize(file_path); }

} // namespace bdd