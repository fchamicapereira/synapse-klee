#pragma once

#include "tofino_module.h"

#include "if.h"
#include "then.h"
#include "else.h"
#include "send_to_controller.h"

namespace synapse {
namespace tofino {

class CachedTableConditionalDelete : public TofinoModule {
private:
  DS_ID cached_table_id;
  std::unordered_set<DS_ID> cached_table_byproducts;

  addr_t obj;
  klee::ref<klee::Expr> key;
  klee::ref<klee::Expr> read_value;
  symbol_t map_has_this_key;
  symbol_t cache_delete_failed;

public:
  CachedTableConditionalDelete(
      const bdd::Node *node, DS_ID _cached_table_id,
      const std::unordered_set<DS_ID> &_cached_table_byproducts, addr_t _obj,
      klee::ref<klee::Expr> _key, klee::ref<klee::Expr> _read_value,
      const symbol_t &_map_has_this_key, const symbol_t &_cache_delete_failed)
      : TofinoModule(ModuleType::Tofino_CachedTableConditionalDelete,
                     "CachedTableConditionalDelete", node),
        cached_table_id(_cached_table_id),
        cached_table_byproducts(_cached_table_byproducts), obj(_obj), key(_key),
        read_value(_read_value), map_has_this_key(_map_has_this_key),
        cache_delete_failed(_cache_delete_failed) {}

  virtual void visit(EPVisitor &visitor, const EP *ep,
                     const EPNode *ep_node) const override {
    visitor.visit(ep, ep_node, this);
  }

  virtual Module *clone() const override {
    Module *cloned = new CachedTableConditionalDelete(
        node, cached_table_id, cached_table_byproducts, obj, key, read_value,
        map_has_this_key, cache_delete_failed);
    return cloned;
  }

  DS_ID get_id() const { return cached_table_id; }
  addr_t get_obj() const { return obj; }
  klee::ref<klee::Expr> get_key() const { return key; }
  klee::ref<klee::Expr> get_read_value() const { return read_value; }
  const symbol_t &get_map_has_this_key() const { return map_has_this_key; }
  const symbol_t &get_cache_delete_failed() const {
    return cache_delete_failed;
  }

  virtual std::unordered_set<DS_ID> get_generated_ds() const override {
    return cached_table_byproducts;
  }
};

class CachedTableConditionalDeleteGenerator : public TofinoModuleGenerator {
public:
  CachedTableConditionalDeleteGenerator()
      : TofinoModuleGenerator(ModuleType::Tofino_CachedTableConditionalDelete,
                              "CachedTableConditionalDelete") {}

protected:
  virtual std::optional<speculation_t>
  speculate(const EP *ep, const bdd::Node *node,
            const Context &ctx) const override {
    if (node->get_type() != bdd::NodeType::CALL) {
      return std::nullopt;
    }

    const bdd::Call *call_node = static_cast<const bdd::Call *>(node);
    const call_t &call = call_node->get_call();

    if (call.function_name != "map_get") {
      return std::nullopt;
    }

    map_coalescing_data_t coalescing_data;
    if (!can_place_cached_table(ep, call_node, coalescing_data)) {
      return std::nullopt;
    }

    std::vector<const bdd::Call *> future_map_erases;
    if (!is_map_get_followed_by_map_erases_on_hit(ep->get_bdd(), call_node,
                                                  future_map_erases)) {
      return std::nullopt;
    }

    cached_table_data_t cached_table_data =
        get_cached_table_data(ep, call_node);

    std::unordered_set<int> allowed_cache_capacities =
        enumerate_cache_table_capacities(cached_table_data.num_entries);

    double chosen_success_estimation = 0;
    int chosen_cache_capacity = 0;

    // We can use a different method for picking the right estimation depending
    // on the time it takes to find a solution.
    for (int cache_capacity : allowed_cache_capacities) {
      double success_estimation = get_cache_delete_success_estimation_rel(
          ep, node, cache_capacity, cached_table_data.num_entries);

      if (success_estimation > chosen_success_estimation) {
        chosen_success_estimation = success_estimation;
        chosen_cache_capacity = cache_capacity;
      }
    }

    bool can_place_cached_table = can_get_or_build_cached_table(
        ep, node, cached_table_data, chosen_cache_capacity);

    if (!can_place_cached_table) {
      return std::nullopt;
    }

    Context new_ctx = ctx;

    const Profiler *profiler = new_ctx.get_profiler();
    constraints_t constraints = node->get_ordered_branch_constraints();

    std::optional<double> fraction = profiler->get_fraction(constraints);
    assert(fraction.has_value());

    double on_fail_fraction =
        fraction.value() * (1 - chosen_success_estimation);

    new_ctx.update_traffic_fractions(TargetType::Tofino, TargetType::TofinoCPU,
                                     on_fail_fraction);

    new_ctx.scale_profiler(constraints, chosen_success_estimation);

    std::vector<const bdd::Node *> ignore_nodes = get_coalescing_nodes_from_key(
        ep->get_bdd(), node, cached_table_data.key, coalescing_data);

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

    if (call.function_name != "map_get") {
      return products;
    }

    map_coalescing_data_t coalescing_data;
    if (!can_place_cached_table(ep, call_node, coalescing_data)) {
      return products;
    }

    std::vector<const bdd::Call *> future_map_erases;
    if (!is_map_get_followed_by_map_erases_on_hit(ep->get_bdd(), call_node,
                                                  future_map_erases)) {
      return products;
    }

    symbol_t cache_delete_failed = create_symbol("cache_delete_failed", 32);

    cached_table_data_t cached_table_data =
        get_cached_table_data(ep, call_node);

    std::unordered_set<int> allowed_cache_capacities =
        enumerate_cache_table_capacities(cached_table_data.num_entries);

    for (int cache_capacity : allowed_cache_capacities) {
      std::optional<__generator_product_t> product =
          concretize_cached_table_cond_delete(
              ep, node, coalescing_data, cached_table_data, cache_delete_failed,
              cache_capacity);

      if (product.has_value()) {
        products.push_back(*product);
      }
    }

    return products;
  }

private:
  std::optional<__generator_product_t> concretize_cached_table_cond_delete(
      const EP *ep, const bdd::Node *node,
      const map_coalescing_data_t &coalescing_data,
      const cached_table_data_t &cached_table_data,
      const symbol_t &cache_delete_failed, int cache_capacity) const {
    std::unordered_set<DS_ID> deps;
    bool already_exists;

    CachedTable *cached_table = get_or_build_cached_table(
        ep, node, cached_table_data, cache_capacity, already_exists, deps);

    if (!cached_table) {
      return std::nullopt;
    }

    std::unordered_set<DS_ID> byproducts =
        get_cached_table_byproducts(cached_table);

    Module *module = new CachedTableConditionalDelete(
        node, cached_table->id, byproducts, cached_table_data.obj,
        cached_table_data.key, cached_table_data.read_value,
        cached_table_data.map_has_this_key, cache_delete_failed);
    EPNode *cached_table_cond_write_node = new EPNode(module);

    EP *new_ep = new EP(*ep);

    klee::ref<klee::Expr> cache_delete_success_condition =
        build_cache_delete_success_condition(cache_delete_failed);

    bdd::Node *on_cache_delete_success;
    bdd::Node *on_cache_delete_failed;
    bdd::BDD *new_bdd = branch_bdd_on_cache_delete_success(
        new_ep, node, cached_table_data, cache_delete_success_condition,
        coalescing_data, on_cache_delete_success, on_cache_delete_failed);

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

    cached_table_cond_write_node->set_children({if_node});
    if_node->set_prev(cached_table_cond_write_node);

    if_node->set_children({then_node, else_node});
    then_node->set_prev(if_node);
    else_node->set_prev(if_node);

    else_node->set_children({send_to_controller_node});
    send_to_controller_node->set_prev(else_node);

    double cache_delete_success_estimation_rel =
        get_cache_delete_success_estimation_rel(ep, node, cache_capacity,
                                                cached_table_data.num_entries);

    new_ep->update_node_constraints(then_node, else_node,
                                    cache_delete_success_condition);
    new_ep->add_hit_rate_estimation(cache_delete_success_condition,
                                    cache_delete_success_estimation_rel);

    if (!already_exists) {
      place_cached_table(new_ep, coalescing_data, cached_table, deps);
    }

    EPLeaf on_cache_delete_success_leaf(then_node, on_cache_delete_success);
    EPLeaf on_cache_delete_failed_leaf(send_to_controller_node,
                                       on_cache_delete_failed);

    new_ep->process_leaf(
        cached_table_cond_write_node,
        {on_cache_delete_success_leaf, on_cache_delete_failed_leaf});
    new_ep->replace_bdd(new_bdd);

    new_ep->inspect_debug();

    std::stringstream descr;
    descr << "cap=" << cache_capacity;

    return __generator_product_t(new_ep, descr.str());
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

    rw_fractions_t rw_fractions =
        get_cond_map_put_rw_profile_fractions(ep, node);

    double relative_delete_fraction = rw_fractions.write / *fraction;
    double relative_read_fraction = rw_fractions.read / *fraction;

    double cache_update_success_fraction =
        static_cast<double>(cache_capacity) / num_entries;

    double relative_cache_success_fraction =
        relative_read_fraction +
        relative_delete_fraction * cache_update_success_fraction;

    return relative_cache_success_fraction;
  }

  std::unordered_set<DS_ID>
  get_cached_table_byproducts(CachedTable *cached_table) const {
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

  cached_table_data_t get_cached_table_data(const EP *ep,
                                            const bdd::Call *map_get) const {
    cached_table_data_t cached_table_data;

    const call_t &get_call = map_get->get_call();

    symbols_t symbols = map_get->get_locally_generated_symbols();
    klee::ref<klee::Expr> obj_expr = get_call.args.at("map").expr;

    cached_table_data.obj = kutil::expr_addr_to_obj_addr(obj_expr);
    cached_table_data.key = get_call.args.at("key").in;
    cached_table_data.read_value = get_call.args.at("value_out").out;

    bool found = get_symbol(symbols, "map_has_this_key",
                            cached_table_data.map_has_this_key);
    assert(found && "Symbol map_has_this_key not found");

    const Context &ctx = ep->get_ctx();
    const bdd::map_config_t &cfg = ctx.get_map_config(cached_table_data.obj);

    cached_table_data.num_entries = cfg.capacity;
    return cached_table_data;
  }

  klee::ref<klee::Expr> build_cache_delete_success_condition(
      const symbol_t &cache_delete_failed) const {
    klee::ref<klee::Expr> zero = kutil::solver_toolbox.exprBuilder->Constant(
        0, cache_delete_failed.expr->getWidth());
    return kutil::solver_toolbox.exprBuilder->Eq(cache_delete_failed.expr,
                                                 zero);
  }

  void add_map_get_clone_on_cache_delete_failed(
      const EP *ep, bdd::BDD *bdd, const bdd::Node *map_get,
      const bdd::Branch *cache_delete_branch,
      bdd::Node *&new_on_cache_delete_failed) const {
    bdd::node_id_t &id = bdd->get_mutable_id();
    bdd::NodeManager &manager = bdd->get_mutable_manager();
    bdd::Node *map_get_on_cache_delete_failed = map_get->clone(manager, false);
    map_get_on_cache_delete_failed->recursive_update_ids(id);

    add_non_branch_nodes_to_bdd(ep, bdd, cache_delete_branch->get_on_false(),
                                {map_get_on_cache_delete_failed},
                                new_on_cache_delete_failed);
  }

  void replicate_hdr_parsing_ops_on_cache_delete_failed(
      const EP *ep, bdd::BDD *bdd, const bdd::Branch *cache_delete_branch,
      bdd::Node *&new_on_cache_delete_failed) const {
    const bdd::Node *on_cache_delete_failed =
        cache_delete_branch->get_on_false();

    std::vector<const bdd::Node *> prev_borrows = get_prev_functions(
        ep, on_cache_delete_failed, {"packet_borrow_next_chunk"});

    if (prev_borrows.empty()) {
      return;
    }

    add_non_branch_nodes_to_bdd(ep, bdd, on_cache_delete_failed, prev_borrows,
                                new_on_cache_delete_failed);
  }

  void delete_coalescing_nodes_on_success(
      const EP *ep, bdd::BDD *bdd, bdd::Node *on_success,
      const map_coalescing_data_t &coalescing_data,
      klee::ref<klee::Expr> key) const {
    const std::vector<const bdd::Node *> targets =
        get_coalescing_nodes_from_key(bdd, on_success, key, coalescing_data);

    for (const bdd::Node *target : targets) {
      bdd::Node *trash;
      delete_non_branch_node_from_bdd(ep, bdd, target->get_id(), trash);
    }
  }

  bdd::BDD *branch_bdd_on_cache_delete_success(
      const EP *ep, const bdd::Node *map_get,
      const cached_table_data_t &cached_table_data,
      klee::ref<klee::Expr> cache_delete_success_condition,
      const map_coalescing_data_t &coalescing_data,
      bdd::Node *&on_cache_delete_success,
      bdd::Node *&on_cache_delete_failed) const {
    const bdd::BDD *old_bdd = ep->get_bdd();
    bdd::BDD *new_bdd = new bdd::BDD(*old_bdd);

    const bdd::Node *next = map_get->get_next();
    assert(next);

    bdd::Branch *cache_delete_branch;
    add_branch_to_bdd(ep, new_bdd, next, cache_delete_success_condition,
                      cache_delete_branch);
    on_cache_delete_success = cache_delete_branch->get_mutable_on_true();

    add_map_get_clone_on_cache_delete_failed(
        ep, new_bdd, map_get, cache_delete_branch, on_cache_delete_failed);
    replicate_hdr_parsing_ops_on_cache_delete_failed(
        ep, new_bdd, cache_delete_branch, on_cache_delete_failed);

    delete_coalescing_nodes_on_success(ep, new_bdd, on_cache_delete_success,
                                       coalescing_data, cached_table_data.key);

    return new_bdd;
  }
};

} // namespace tofino
} // namespace synapse
