#pragma once

#include "tofino_module.h"

namespace synapse {
namespace tofino {

class TTLCachedTableDelete : public TofinoModule {
private:
  DS_ID cached_table_id;
  std::unordered_set<DS_ID> cached_table_byproducts;

  addr_t obj;
  klee::ref<klee::Expr> key;
  symbol_t cached_delete_failed;

public:
  TTLCachedTableDelete(
      const bdd::Node *node, DS_ID _cached_table_id,
      const std::unordered_set<DS_ID> &_cached_table_byproducts, addr_t _obj,
      klee::ref<klee::Expr> _key, const symbol_t &_cached_delete_failed)
      : TofinoModule(ModuleType::Tofino_TTLCachedTableDelete,
                     "TTLCachedTableDelete", node),
        cached_table_id(_cached_table_id),
        cached_table_byproducts(_cached_table_byproducts), obj(_obj), key(_key),
        cached_delete_failed(_cached_delete_failed) {}

  virtual void visit(EPVisitor &visitor, const EP *ep,
                     const EPNode *ep_node) const override {
    visitor.visit(ep, ep_node, this);
  }

  virtual Module *clone() const override {
    Module *cloned =
        new TTLCachedTableDelete(node, cached_table_id, cached_table_byproducts,
                                 obj, key, cached_delete_failed);
    return cloned;
  }

  DS_ID get_id() const { return cached_table_id; }
  addr_t get_obj() const { return obj; }
  klee::ref<klee::Expr> get_key() const { return key; }
  const symbol_t &get_cached_delete_failed() const {
    return cached_delete_failed;
  }

  virtual std::unordered_set<DS_ID> get_generated_ds() const override {
    return cached_table_byproducts;
  }
};

class TTLCachedTableDeleteGenerator : public TofinoModuleGenerator {
public:
  TTLCachedTableDeleteGenerator()
      : TofinoModuleGenerator(ModuleType::Tofino_TTLCachedTableDelete,
                              "TTLCachedTableDelete") {}

protected:
  virtual std::optional<speculation_t>
  speculate(const EP *ep, const bdd::Node *node,
            const Context &ctx) const override {
    if (node->get_type() != bdd::NodeType::CALL) {
      return std::nullopt;
    }

    const bdd::Call *call_node = static_cast<const bdd::Call *>(node);
    const call_t &call = call_node->get_call();

    if (call.function_name != "map_erase" &&
        call.function_name != "dchain_free_index") {
      return std::nullopt;
    }

    const bdd::Call *map_erase;

    if (call.function_name == "dchain_free_index") {
      map_erase = get_map_erase_after_dchain_free(ep, call_node);
    } else {
      map_erase = call_node;
    }

    if (!map_erase) {
      return std::nullopt;
    }

    map_coalescing_data_t coalescing_data;
    if (!get_map_coalescing_data_from_map_op(ep, map_erase, coalescing_data)) {
      return std::nullopt;
    }

    if (!can_place_cached_table(ep, coalescing_data)) {
      return std::nullopt;
    }

    cached_table_data_t cached_table_data =
        get_cached_table_data(ep, map_erase);

    std::unordered_set<int> allowed_cache_capacities =
        enumerate_cache_table_capacities(cached_table_data.num_entries);

    double chosen_success_estimation = 0;
    int chosen_cache_capacity = 0;
    bool successfully_placed = false;

    // We can use a different method for picking the right estimation depending
    // on the time it takes to find a solution.
    for (int cache_capacity : allowed_cache_capacities) {
      double success_estimation = get_cache_delete_success_estimation_rel(
          ep, node, cache_capacity, cached_table_data.num_entries);

      if (!can_get_or_build_cached_table(ep, node, cached_table_data,
                                         chosen_cache_capacity)) {
        continue;
      }

      if (success_estimation > chosen_success_estimation) {
        chosen_success_estimation = success_estimation;
        chosen_cache_capacity = cache_capacity;
      }

      successfully_placed = true;
    }

    if (!successfully_placed) {
      return std::nullopt;
    }

    Context new_ctx = ctx;
    const Profiler *profiler = new_ctx.get_profiler();
    constraints_t constraints = node->get_ordered_branch_constraints();

    std::optional<double> fraction = profiler->get_fraction(constraints);
    assert(fraction.has_value());

    double on_fail_fraction = *fraction * (1 - chosen_success_estimation);

    new_ctx.update_traffic_fractions(TargetType::Tofino, TargetType::TofinoCPU,
                                     on_fail_fraction);

    new_ctx.scale_profiler(constraints, chosen_success_estimation);

    std::vector<const bdd::Node *> ignore_nodes =
        get_future_related_nodes(ep, node, coalescing_data);

    speculation_t speculation(new_ctx);
    for (const bdd::Node *op : ignore_nodes) {
      speculation.skip.insert(op->get_id());
    }

    return speculation;
  }

  virtual std::vector<__generator_product_t>
  process_node(const EP *ep, const bdd::Node *node) const override {
    std::vector<__generator_product_t> products;

    if (node->get_type() != bdd::NodeType::CALL) {
      return products;
    }

    const bdd::Call *call_node = static_cast<const bdd::Call *>(node);
    const call_t &call = call_node->get_call();

    if (call.function_name != "map_erase" &&
        call.function_name != "dchain_free_index") {
      return products;
    }

    const bdd::Call *map_erase;
    std::optional<const bdd::Node *> dchain_free_index;

    if (call.function_name == "dchain_free_index") {
      map_erase = get_map_erase_after_dchain_free(ep, call_node);
      dchain_free_index = call_node;
    } else {
      map_erase = call_node;
    }

    if (!map_erase) {
      return products;
    }

    // A bit too strict, but I don't want to build the cached table ds in this
    // module because we don't have a value.
    if (!check_placement(ep, map_erase, "map",
                         PlacementDecision::Tofino_TTLCachedTable)) {
      return products;
    }

    cached_table_data_t cached_table_data =
        get_cached_table_data(ep, map_erase);

    symbol_t cache_delete_failed = create_symbol("cache_delete_failed", 32);

    std::optional<__generator_product_t> product =
        concretize_cached_table_delete(ep, node, map_erase, dchain_free_index,
                                       cached_table_data, cache_delete_failed);

    if (product.has_value()) {
      products.push_back(*product);
    }

    return products;
  }

private:
  std::optional<__generator_product_t> concretize_cached_table_delete(
      const EP *ep, const bdd::Node *node, const bdd::Node *map_erase,
      const std::optional<const bdd::Node *> &dchain_free_index,
      const cached_table_data_t &cached_table_data,
      const symbol_t &cache_delete_failed) const {
    std::unordered_set<DS_ID> deps;

    TTLCachedTable *cached_table =
        get_cached_table(ep, map_erase, cached_table_data, deps);

    if (!cached_table) {
      return std::nullopt;
    }

    std::unordered_set<DS_ID> byproducts =
        get_cached_table_byproducts(cached_table);

    klee::ref<klee::Expr> cache_delete_success_condition =
        build_cache_delete_success_condition(cache_delete_failed);

    Module *module = new TTLCachedTableDelete(
        node, cached_table->id, byproducts, cached_table_data.obj,
        cached_table_data.key, cache_delete_failed);
    EPNode *cached_table_delete_node = new EPNode(module);

    EP *new_ep = new EP(*ep);

    bdd::Node *on_cache_delete_success;
    bdd::Node *on_cache_delete_failed;
    std::optional<constraints_t> deleted_branch_constraints;

    bdd::BDD *new_bdd = branch_bdd_on_cache_delete_success(
        new_ep, node, map_erase, dchain_free_index, cached_table_data,
        cache_delete_success_condition, on_cache_delete_success,
        on_cache_delete_failed, deleted_branch_constraints);

    symbols_t symbols = get_dataplane_state(ep, node);

    Module *if_module = new If(node, cache_delete_success_condition,
                               {cache_delete_success_condition});
    Module *then_module = new Then(node);
    Module *else_module = new Else(node);
    Module *send_to_controller_module =
        new SendToController(on_cache_delete_failed, symbols);

    EPNode *if_node = new EPNode(if_module);
    EPNode *then_node = new EPNode(then_module);
    EPNode *else_node = new EPNode(else_module);
    EPNode *send_to_controller_node = new EPNode(send_to_controller_module);

    cached_table_delete_node->set_children({if_node});
    if_node->set_prev(cached_table_delete_node);

    if_node->set_children({then_node, else_node});
    then_node->set_prev(if_node);
    else_node->set_prev(if_node);

    else_node->set_children({send_to_controller_node});
    send_to_controller_node->set_prev(else_node);

    double cache_delete_success_estimation_rel =
        get_cache_delete_success_estimation_rel(ep, node,
                                                cached_table->cache_capacity,
                                                cached_table_data.num_entries);

    new_ep->update_node_constraints(then_node, else_node,
                                    cache_delete_success_condition);

    new_ep->add_hit_rate_estimation(cache_delete_success_condition,
                                    cache_delete_success_estimation_rel);

    if (deleted_branch_constraints.has_value()) {
      new_ep->remove_hit_rate_node(deleted_branch_constraints.value());
    }

    EPLeaf on_cache_delete_success_leaf(then_node, on_cache_delete_success);
    EPLeaf on_cache_delete_failed_leaf(send_to_controller_node,
                                       on_cache_delete_failed);

    new_ep->process_leaf(
        cached_table_delete_node,
        {on_cache_delete_success_leaf, on_cache_delete_failed_leaf});
    new_ep->replace_bdd(new_bdd);
    // new_ep->inspect();

    std::stringstream descr;
    descr << "capacity=" << cached_table->cache_capacity;

    return __generator_product_t(new_ep, descr.str());
  }

  klee::ref<klee::Expr> build_cache_delete_success_condition(
      const symbol_t &cache_delete_failed) const {
    klee::ref<klee::Expr> zero = kutil::solver_toolbox.exprBuilder->Constant(
        0, cache_delete_failed.expr->getWidth());
    return kutil::solver_toolbox.exprBuilder->Eq(cache_delete_failed.expr,
                                                 zero);
  }

  double get_cache_delete_success_estimation_rel(const EP *ep,
                                                 const bdd::Node *node,
                                                 int cache_capacity,
                                                 int num_entries) const {
    const Context &ctx = ep->get_ctx();
    const Profiler *profiler = ctx.get_profiler();
    constraints_t constraints = node->get_ordered_branch_constraints();

    std::optional<double> fraction = profiler->get_fraction(constraints);
    assert(fraction.has_value());

    double cache_update_success_fraction =
        static_cast<double>(cache_capacity) / num_entries;

    double relative_cache_success_fraction =
        *fraction * cache_update_success_fraction;

    return relative_cache_success_fraction;
  }

  std::unordered_set<DS_ID>
  get_cached_table_byproducts(TTLCachedTable *cached_table) const {
    std::unordered_set<DS_ID> byproducts;

    std::vector<std::unordered_set<const DS *>> internal_ds =
        cached_table->get_internal_ds();
    for (const std::unordered_set<const DS *> &ds_set : internal_ds) {
      for (const DS *ds : ds_set) {
        byproducts.insert(ds->id);
      }
    }

    return byproducts;
  }

  const bdd::Call *
  get_map_erase_after_dchain_free(const EP *ep,
                                  const bdd::Call *dchain_free_index) const {
    const call_t &dchain_free_index_call = dchain_free_index->get_call();
    assert(dchain_free_index_call.function_name == "dchain_free_index");

    klee::ref<klee::Expr> chain_addr_expr =
        dchain_free_index_call.args.at("chain").expr;
    addr_t chain_obj = kutil::expr_addr_to_obj_addr(chain_addr_expr);

    const Context &ctx = ep->get_ctx();
    std::optional<map_coalescing_data_t> cached_table_data =
        ctx.get_coalescing_data(chain_obj);

    if (!cached_table_data.has_value()) {
      return nullptr;
    }

    std::vector<const bdd::Call *> future_map_erase =
        get_future_functions(dchain_free_index, {"map_erase"});

    if (future_map_erase.empty()) {
      return nullptr;
    }

    for (const bdd::Call *map_erase : future_map_erase) {
      const call_t &map_erase_call = map_erase->get_call();

      klee::ref<klee::Expr> map_addr_expr = map_erase_call.args.at("map").expr;
      addr_t map_obj = kutil::expr_addr_to_obj_addr(map_addr_expr);

      if (map_obj == cached_table_data->map) {
        return map_erase;
      }
    }

    return nullptr;
  }

  cached_table_data_t get_cached_table_data(const EP *ep,
                                            const bdd::Call *map_erase) const {
    cached_table_data_t cached_table_data;

    const call_t &call = map_erase->get_call();
    assert(call.function_name == "map_erase");

    klee::ref<klee::Expr> map_addr_expr = call.args.at("map").expr;
    klee::ref<klee::Expr> key = call.args.at("key").in;

    cached_table_data.obj = kutil::expr_addr_to_obj_addr(map_addr_expr);
    cached_table_data.key = key;

    const Context &ctx = ep->get_ctx();
    const bdd::map_config_t &cfg = ctx.get_map_config(cached_table_data.obj);

    cached_table_data.num_entries = cfg.capacity;

    return cached_table_data;
  }

  std::vector<const bdd::Node *> get_future_related_nodes(
      const EP *ep, const bdd::Node *node,
      const map_coalescing_data_t &cached_table_data) const {
    std::vector<const bdd::Call *> ops =
        get_future_functions(node, {"dchain_free_index", "map_erase"});

    std::vector<const bdd::Node *> related_ops;
    for (const bdd::Call *op : ops) {
      const call_t &call = op->get_call();

      if (call.function_name == "dchain_free_index") {
        klee::ref<klee::Expr> obj_expr = call.args.at("chain").expr;
        addr_t obj = kutil::expr_addr_to_obj_addr(obj_expr);

        if (obj != cached_table_data.dchain) {
          continue;
        }
      } else if (call.function_name == "map_erase") {
        klee::ref<klee::Expr> obj_expr = call.args.at("map").expr;
        addr_t obj = kutil::expr_addr_to_obj_addr(obj_expr);

        if (obj != cached_table_data.map) {
          continue;
        }
      } else {
        klee::ref<klee::Expr> obj_expr = call.args.at("vector").expr;
        addr_t obj = kutil::expr_addr_to_obj_addr(obj_expr);

        if (obj != cached_table_data.vector_key) {
          continue;
        }
      }

      related_ops.push_back(op);
    }

    return related_ops;
  }

  void replicate_hdr_parsing_ops_on_cache_delete_failed(
      const EP *ep, bdd::BDD *bdd, const bdd::Branch *cache_delete_branch,
      bdd::Node *&new_on_cache_delete_failed) const {
    const bdd::Node *on_cache_delete_failed =
        cache_delete_branch->get_on_false();

    std::vector<const bdd::Call *> prev_borrows = get_prev_functions(
        ep, on_cache_delete_failed, {"packet_borrow_next_chunk"});

    if (prev_borrows.empty()) {
      return;
    }

    std::vector<const bdd::Node *> non_branch_nodes_to_add;
    for (const bdd::Call *prev_borrow : prev_borrows) {
      non_branch_nodes_to_add.push_back(prev_borrow);
    }

    add_non_branch_nodes_to_bdd(ep, bdd, on_cache_delete_failed,
                                non_branch_nodes_to_add,
                                new_on_cache_delete_failed);
  }

  void add_delete_clones_on_fail(
      const EP *ep, bdd::BDD *bdd, const bdd::Branch *cache_delete_branch,
      const bdd::Node *map_erase,
      const std::optional<const bdd::Node *> &dchain_free_index,
      bdd::Node *&new_on_cache_delete_failed) const {
    bdd::node_id_t &id = bdd->get_mutable_id();
    bdd::NodeManager &manager = bdd->get_mutable_manager();

    std::vector<const bdd::Node *> new_nodes;

    bdd::Node *map_erase_on_fail = map_erase->clone(manager, false);
    map_erase_on_fail->recursive_update_ids(id);
    new_nodes.push_back(map_erase_on_fail);

    if (dchain_free_index.has_value()) {
      bdd::Node *dchain_free_index_on_fail =
          (*dchain_free_index)->clone(manager, false);
      dchain_free_index_on_fail->recursive_update_ids(id);
      new_nodes.push_back(dchain_free_index_on_fail);
    }

    add_non_branch_nodes_to_bdd(ep, bdd, cache_delete_branch->get_on_false(),
                                new_nodes, new_on_cache_delete_failed);
  }

  void delete_future_related_nodes(const EP *ep, bdd::BDD *bdd,
                                   bdd::Node *&node, addr_t map_obj) const {
    const Context &ctx = ep->get_ctx();
    std::optional<map_coalescing_data_t> cached_table_data =
        ctx.get_coalescing_data(map_obj);
    assert(cached_table_data.has_value());

    std::vector<const bdd::Node *> ops =
        get_future_related_nodes(ep, node, *cached_table_data);

    for (const bdd::Node *op : ops) {
      bool replace = (op == node);

      bdd::Node *replacement;
      delete_non_branch_node_from_bdd(ep, bdd, op->get_id(), replacement);

      if (replace) {
        node = replacement;
      }
    }
  }

  bdd::BDD *branch_bdd_on_cache_delete_success(
      const EP *ep, const bdd::Node *node, const bdd::Node *map_erase,
      const std::optional<const bdd::Node *> &dchain_free_index,
      const cached_table_data_t &cached_table_data,
      klee::ref<klee::Expr> cache_delete_success_condition,
      bdd::Node *&on_cache_delete_success, bdd::Node *&on_cache_delete_failed,
      std::optional<constraints_t> &deleted_branch_constraints) const {
    const bdd::BDD *old_bdd = ep->get_bdd();
    bdd::BDD *new_bdd = new bdd::BDD(*old_bdd);

    const bdd::Node *next = node->get_next();
    assert(next);

    bdd::Branch *cache_delete_branch;
    add_branch_to_bdd(ep, new_bdd, next, cache_delete_success_condition,
                      cache_delete_branch);

    on_cache_delete_success = cache_delete_branch->get_mutable_on_true();
    on_cache_delete_failed = cache_delete_branch->get_mutable_on_false();

    delete_future_related_nodes(ep, new_bdd, on_cache_delete_success,
                                cached_table_data.obj);
    delete_future_related_nodes(ep, new_bdd, on_cache_delete_failed,
                                cached_table_data.obj);

    add_delete_clones_on_fail(ep, new_bdd, cache_delete_branch, map_erase,
                              dchain_free_index, on_cache_delete_failed);
    replicate_hdr_parsing_ops_on_cache_delete_failed(
        ep, new_bdd, cache_delete_branch, on_cache_delete_failed);

    return new_bdd;
  }
};

} // namespace tofino
} // namespace synapse
