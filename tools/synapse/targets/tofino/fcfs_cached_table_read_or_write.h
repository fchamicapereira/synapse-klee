#pragma once

#include "tofino_module.h"

#include "if.h"
#include "then.h"
#include "else.h"
#include "send_to_controller.h"

namespace synapse {
namespace tofino {

class FCFSCachedTableReadOrWrite : public TofinoModule {
private:
  DS_ID cached_table_id;
  std::unordered_set<DS_ID> cached_table_byproducts;

  addr_t obj;
  klee::ref<klee::Expr> key;
  klee::ref<klee::Expr> read_value;
  klee::ref<klee::Expr> write_value;
  symbol_t map_has_this_key;
  symbol_t cache_write_failed;

public:
  FCFSCachedTableReadOrWrite(
      const bdd::Node *node, DS_ID _cached_table_id,
      const std::unordered_set<DS_ID> &_cached_table_byproducts, addr_t _obj,
      klee::ref<klee::Expr> _key, klee::ref<klee::Expr> _read_value,
      klee::ref<klee::Expr> _write_value, const symbol_t &_map_has_this_key,
      const symbol_t &_cache_write_failed)
      : TofinoModule(ModuleType::Tofino_FCFSCachedTableReadOrWrite,
                     "FCFSCachedTableReadOrWrite", node),
        cached_table_id(_cached_table_id),
        cached_table_byproducts(_cached_table_byproducts), obj(_obj), key(_key),
        read_value(_read_value), write_value(_write_value),
        map_has_this_key(_map_has_this_key),
        cache_write_failed(_cache_write_failed) {}

  virtual void visit(EPVisitor &visitor, const EP *ep,
                     const EPNode *ep_node) const override {
    visitor.visit(ep, ep_node, this);
  }

  virtual Module *clone() const override {
    Module *cloned = new FCFSCachedTableReadOrWrite(
        node, cached_table_id, cached_table_byproducts, obj, key, read_value,
        write_value, map_has_this_key, cache_write_failed);
    return cloned;
  }

  DS_ID get_id() const { return cached_table_id; }
  addr_t get_obj() const { return obj; }
  klee::ref<klee::Expr> get_key() const { return key; }
  klee::ref<klee::Expr> get_read_value() const { return read_value; }
  klee::ref<klee::Expr> get_write_value() const { return write_value; }
  const symbol_t &get_map_has_this_key() const { return map_has_this_key; }
  const symbol_t &get_cache_write_failed() const { return cache_write_failed; }

  virtual std::unordered_set<DS_ID> get_generated_ds() const override {
    return cached_table_byproducts;
  }
};

class FCFSCachedTableReadOrWriteGenerator : public TofinoModuleGenerator {
public:
  FCFSCachedTableReadOrWriteGenerator()
      : TofinoModuleGenerator(ModuleType::Tofino_FCFSCachedTableReadOrWrite,
                              "FCFSCachedTableReadOrWrite") {}

protected:
  virtual std::optional<speculation_t>
  speculate(const EP *ep, const bdd::Node *node,
            const Context &ctx) const override {
    if (node->get_type() != bdd::NodeType::CALL) {
      return std::nullopt;
    }

    const bdd::Call *map_get = static_cast<const bdd::Call *>(node);

    std::vector<const bdd::Call *> future_map_puts;
    if (!is_map_get_followed_by_map_puts_on_miss(ep->get_bdd(), map_get,
                                                 future_map_puts)) {
      // The cached table read should deal with these cases.
      return std::nullopt;
    }

    map_coalescing_objs_t map_objs;
    if (!get_map_coalescing_objs_from_map_op(ep, map_get, map_objs)) {
      return std::nullopt;
    }

    if (!can_place_fcfs_cached_table(ep, map_objs)) {
      return std::nullopt;
    }

    fcfs_cached_table_data_t cached_table_data =
        get_fcfs_cached_table_data(ep, map_get, future_map_puts);

    std::unordered_set<int> allowed_cache_capacities =
        enumerate_fcfs_cache_table_capacities(cached_table_data.num_entries);

    double chosen_success_estimation = 0;
    int chosen_cache_capacity = 0;
    bool successfully_placed = false;

    // We can use a different method for picking the right estimation depending
    // on the time it takes to find a solution.
    for (int cache_capacity : allowed_cache_capacities) {
      double success_estimation = get_cache_success_estimation_rel(
          ep, node, cached_table_data.key, cache_capacity);

      if (!can_get_or_build_fcfs_cached_table(
              ep, node, cached_table_data.obj, cached_table_data.key,
              cached_table_data.num_entries, chosen_cache_capacity)) {
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
        get_nodes_to_speculatively_ignore(ep, map_get, map_objs,
                                          cached_table_data.key);

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

    const bdd::Call *map_get = static_cast<const bdd::Call *>(node);

    std::vector<const bdd::Call *> future_map_puts;
    if (!is_map_get_followed_by_map_puts_on_miss(ep->get_bdd(), map_get,
                                                 future_map_puts)) {
      // The cached table read should deal with these cases.
      return products;
    }

    map_coalescing_objs_t map_objs;
    if (!get_map_coalescing_objs_from_map_op(ep, map_get, map_objs)) {
      return products;
    }

    if (!can_place_fcfs_cached_table(ep, map_objs)) {
      return products;
    }

    symbol_t cache_write_failed = create_symbol("cache_write_failed", 32);

    fcfs_cached_table_data_t cached_table_data =
        get_fcfs_cached_table_data(ep, map_get, future_map_puts);

    std::unordered_set<int> allowed_cache_capacities =
        enumerate_fcfs_cache_table_capacities(cached_table_data.num_entries);

    for (int cache_capacity : allowed_cache_capacities) {
      std::optional<__generator_product_t> product =
          concretize_cached_table_cond_write(
              ep, node, map_objs, cached_table_data, cache_write_failed,
              cache_capacity);

      if (product.has_value()) {
        products.push_back(*product);
      }
    }

    return products;
  }

private:
  struct fcfs_cached_table_data_t {
    addr_t obj;
    klee::ref<klee::Expr> key;
    klee::ref<klee::Expr> read_value;
    klee::ref<klee::Expr> write_value;
    symbol_t map_has_this_key;
    int num_entries;
  };

  std::optional<__generator_product_t> concretize_cached_table_cond_write(
      const EP *ep, const bdd::Node *node,
      const map_coalescing_objs_t &map_objs,
      const fcfs_cached_table_data_t &fcfs_cached_table_data,
      const symbol_t &cache_write_failed, int cache_capacity) const {
    FCFSCachedTable *cached_table = build_or_reuse_fcfs_cached_table(
        ep, node, fcfs_cached_table_data.obj, fcfs_cached_table_data.key,
        fcfs_cached_table_data.num_entries, cache_capacity);

    if (!cached_table) {
      return std::nullopt;
    }

    std::unordered_set<DS_ID> byproducts =
        get_cached_table_byproducts(cached_table);

    klee::ref<klee::Expr> cache_write_success_condition =
        build_cache_write_success_condition(cache_write_failed);

    Module *module = new FCFSCachedTableReadOrWrite(
        node, cached_table->id, byproducts, fcfs_cached_table_data.obj,
        fcfs_cached_table_data.key, fcfs_cached_table_data.read_value,
        fcfs_cached_table_data.write_value,
        fcfs_cached_table_data.map_has_this_key, cache_write_failed);
    EPNode *cached_table_cond_write_node = new EPNode(module);

    EP *new_ep = new EP(*ep);

    bdd::Node *on_cache_write_success;
    bdd::Node *on_cache_write_failed;
    std::optional<constraints_t> deleted_branch_constraints;

    bdd::BDD *new_bdd = branch_bdd_on_cache_write_success(
        new_ep, node, fcfs_cached_table_data, cache_write_success_condition,
        map_objs, on_cache_write_success, on_cache_write_failed,
        deleted_branch_constraints);

    symbols_t symbols = get_dataplane_state(ep, node);

    Module *if_module = new If(node, cache_write_success_condition,
                               {cache_write_success_condition});
    Module *then_module = new Then(node);
    Module *else_module = new Else(node);
    Module *send_to_controller_module =
        new SendToController(on_cache_write_failed, symbols);

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

    double cache_write_success_estimation_rel =
        get_cache_success_estimation_rel(ep, node, fcfs_cached_table_data.key,
                                         cache_capacity);

    new_ep->update_node_constraints(then_node, else_node,
                                    cache_write_success_condition);

    new_ep->add_hit_rate_estimation(cache_write_success_condition,
                                    cache_write_success_estimation_rel);

    if (deleted_branch_constraints.has_value()) {
      new_ep->remove_hit_rate_node(deleted_branch_constraints.value());
    }

    // new_bdd->inspect();

    // bdd::BDDVisualizer::visualize(new_bdd, false);
    // ProfilerVisualizer::visualize(ep->get_bdd(),
    // ep->get_ctx().get_profiler(),
    //                               false);
    // ProfilerVisualizer::visualize(new_bdd, new_ep->get_ctx().get_profiler(),
    //                               true);

    place_fcfs_cached_table(new_ep, node, map_objs, cached_table);

    EPLeaf on_cache_write_success_leaf(then_node, on_cache_write_success);
    EPLeaf on_cache_write_failed_leaf(send_to_controller_node,
                                      on_cache_write_failed);

    new_ep->process_leaf(
        cached_table_cond_write_node,
        {on_cache_write_success_leaf, on_cache_write_failed_leaf});
    new_ep->replace_bdd(new_bdd);
    // new_ep->inspect();

    std::stringstream descr;
    descr << "capacity=" << cache_capacity;
    descr << " hit-rate=" << cache_write_success_estimation_rel;

    return __generator_product_t(new_ep, descr.str());
  }

  double get_cache_success_estimation_rel(const EP *ep, const bdd::Node *node,
                                          klee::ref<klee::Expr> key,
                                          int cache_capacity) const {
    const Context &ctx = ep->get_ctx();
    const Profiler *profiler = ctx.get_profiler();
    constraints_t constraints = node->get_ordered_branch_constraints();

    std::optional<double> fraction = profiler->get_fraction(constraints);
    assert(fraction.has_value());

    std::optional<FlowStats> flow_stats =
        profiler->get_flow_stats(constraints, key);
    assert(flow_stats.has_value());

    rw_fractions_t rw_fractions =
        get_cond_map_put_rw_profile_fractions(ep, node);

    double relative_write_fraction = rw_fractions.write / *fraction;
    double relative_read_fraction = rw_fractions.read / *fraction;

    uint64_t cached_packets =
        std::min(flow_stats->total_packets,
                 flow_stats->avg_pkts_per_flow * cache_capacity);
    double expected_cached_fraction =
        cached_packets / static_cast<double>(flow_stats->total_packets);

    double relative_cache_success_fraction =
        relative_read_fraction +
        relative_write_fraction * expected_cached_fraction;

    // std::cerr << "Reads: " << relative_read_fraction << std::endl;
    // std::cerr << "Writes: " << relative_write_fraction << std::endl;
    // std::cerr << "Avg pkts per flow: " << flow_stats->avg_pkts_per_flow
    //           << std::endl;
    // std::cerr << "Total flows: " << flow_stats->total_flows << std::endl;
    // std::cerr << "Total packets: " << flow_stats->total_packets << std::endl;
    // std::cerr << "Cached packets: " << cached_packets << std::endl;
    // std::cerr << "Expected cached fraction: " << expected_cached_fraction
    //           << std::endl;
    // std::cerr << "Relative cache success fraction: "
    //           << relative_cache_success_fraction << std::endl;

    return relative_cache_success_fraction;
  }

  std::unordered_set<DS_ID>
  get_cached_table_byproducts(FCFSCachedTable *cached_table) const {
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

  fcfs_cached_table_data_t get_fcfs_cached_table_data(
      const EP *ep, const bdd::Call *map_get,
      std::vector<const bdd::Call *> future_map_puts) const {
    fcfs_cached_table_data_t cached_table_data;

    assert(!future_map_puts.empty());
    const bdd::Call *map_put = future_map_puts.front();

    const call_t &get_call = map_get->get_call();
    const call_t &put_call = map_put->get_call();

    symbols_t symbols = map_get->get_locally_generated_symbols();
    klee::ref<klee::Expr> obj_expr = get_call.args.at("map").expr;

    cached_table_data.obj = kutil::expr_addr_to_obj_addr(obj_expr);
    cached_table_data.key = get_call.args.at("key").in;
    cached_table_data.read_value = get_call.args.at("value_out").out;
    cached_table_data.write_value = put_call.args.at("value").expr;

    bool found = get_symbol(symbols, "map_has_this_key",
                            cached_table_data.map_has_this_key);
    assert(found && "Symbol map_has_this_key not found");

    const Context &ctx = ep->get_ctx();
    const bdd::map_config_t &cfg = ctx.get_map_config(cached_table_data.obj);

    cached_table_data.num_entries = cfg.capacity;
    return cached_table_data;
  }

  klee::ref<klee::Expr> build_cache_write_success_condition(
      const symbol_t &cache_write_failed) const {
    klee::ref<klee::Expr> zero = kutil::solver_toolbox.exprBuilder->Constant(
        0, cache_write_failed.expr->getWidth());
    return kutil::solver_toolbox.exprBuilder->Eq(cache_write_failed.expr, zero);
  }

  void add_map_get_clone_on_cache_write_failed(
      const EP *ep, bdd::BDD *bdd, const bdd::Node *map_get,
      const bdd::Branch *cache_write_branch,
      bdd::Node *&new_on_cache_write_failed) const {
    bdd::node_id_t &id = bdd->get_mutable_id();
    bdd::NodeManager &manager = bdd->get_mutable_manager();
    bdd::Node *map_get_on_cache_write_failed = map_get->clone(manager, false);
    map_get_on_cache_write_failed->recursive_update_ids(id);

    add_non_branch_nodes_to_bdd(ep, bdd, cache_write_branch->get_on_false(),
                                {map_get_on_cache_write_failed},
                                new_on_cache_write_failed);
  }

  void replicate_hdr_parsing_ops_on_cache_write_failed(
      const EP *ep, bdd::BDD *bdd, const bdd::Branch *cache_write_branch,
      bdd::Node *&new_on_cache_write_failed) const {
    const bdd::Node *on_cache_write_failed = cache_write_branch->get_on_false();

    std::vector<const bdd::Call *> prev_borrows = get_prev_functions(
        ep, on_cache_write_failed, {"packet_borrow_next_chunk"});

    if (prev_borrows.empty()) {
      return;
    }

    std::vector<const bdd::Node *> non_branch_nodes_to_add;
    for (const bdd::Call *prev_borrow : prev_borrows) {
      non_branch_nodes_to_add.push_back(prev_borrow);
    }

    add_non_branch_nodes_to_bdd(ep, bdd, on_cache_write_failed,
                                non_branch_nodes_to_add,
                                new_on_cache_write_failed);
  }

  std::vector<const bdd::Node *>
  get_nodes_to_speculatively_ignore(const EP *ep, const bdd::Node *on_success,
                                    const map_coalescing_objs_t &map_objs,
                                    klee::ref<klee::Expr> key) const {
    std::vector<const bdd::Node *> nodes_to_ignore;

    const bdd::BDD *bdd = ep->get_bdd();
    std::vector<const bdd::Call *> coalescing_nodes =
        get_coalescing_nodes_from_key(bdd, on_success, key, map_objs);

    for (const bdd::Call *coalescing_node : coalescing_nodes) {
      nodes_to_ignore.push_back(coalescing_node);

      const call_t &call = coalescing_node->get_call();

      if (call.function_name == "dchain_allocate_new_index") {
        symbol_t out_of_space;
        bool found =
            get_symbol(coalescing_node->get_locally_generated_symbols(),
                       "out_of_space", out_of_space);
        assert(found && "Symbol out_of_space not found");

        const bdd::Branch *branch =
            find_branch_checking_index_alloc(ep, on_success, out_of_space);

        if (branch) {
          nodes_to_ignore.push_back(branch);

          bool direction_to_keep = true;
          const bdd::Node *next = direction_to_keep ? branch->get_on_false()
                                                    : branch->get_on_true();

          next->visit_nodes([&nodes_to_ignore](const bdd::Node *node) {
            nodes_to_ignore.push_back(node);
            return bdd::NodeVisitAction::VISIT_CHILDREN;
          });
        }
      }
    }

    return nodes_to_ignore;
  }

  void delete_coalescing_nodes_on_success(
      const EP *ep, bdd::BDD *bdd, bdd::Node *on_success,
      const map_coalescing_objs_t &map_objs, klee::ref<klee::Expr> key,
      std::optional<constraints_t> &deleted_branch_constraints) const {
    const std::vector<const bdd::Call *> targets =
        get_coalescing_nodes_from_key(bdd, on_success, key, map_objs);

    for (const bdd::Node *target : targets) {
      const bdd::Call *call_target = static_cast<const bdd::Call *>(target);
      const call_t &call = call_target->get_call();

      if (call.function_name == "dchain_allocate_new_index") {
        symbol_t out_of_space;
        bool found = get_symbol(call_target->get_locally_generated_symbols(),
                                "out_of_space", out_of_space);
        assert(found && "Symbol out_of_space not found");

        const bdd::Branch *branch =
            find_branch_checking_index_alloc(ep, on_success, out_of_space);

        if (branch) {
          assert(!deleted_branch_constraints.has_value() &&
                 "Multiple branch checking index allocation detected");
          deleted_branch_constraints = branch->get_ordered_branch_constraints();
          bool direction_to_keep = true;

          klee::ref<klee::Expr> extra_constraint = branch->get_condition();

          // If we want to keep the direction on true, we must remove the on
          // false.
          if (direction_to_keep) {
            extra_constraint =
                kutil::solver_toolbox.exprBuilder->Not(extra_constraint);
          }

          deleted_branch_constraints->push_back(extra_constraint);

          bdd::Node *trash;
          delete_branch_node_from_bdd(ep, bdd, branch->get_id(),
                                      direction_to_keep, trash);
        }
      }

      bdd::Node *trash;
      delete_non_branch_node_from_bdd(ep, bdd, target->get_id(), trash);
    }
  }

  bdd::BDD *branch_bdd_on_cache_write_success(
      const EP *ep, const bdd::Node *map_get,
      const fcfs_cached_table_data_t &fcfs_cached_table_data,
      klee::ref<klee::Expr> cache_write_success_condition,
      const map_coalescing_objs_t &map_objs, bdd::Node *&on_cache_write_success,
      bdd::Node *&on_cache_write_failed,
      std::optional<constraints_t> &deleted_branch_constraints) const {
    const bdd::BDD *old_bdd = ep->get_bdd();
    bdd::BDD *new_bdd = new bdd::BDD(*old_bdd);

    const bdd::Node *next = map_get->get_next();
    assert(next);

    bdd::Branch *cache_write_branch;
    add_branch_to_bdd(ep, new_bdd, next, cache_write_success_condition,
                      cache_write_branch);

    on_cache_write_success = cache_write_branch->get_mutable_on_true();
    add_map_get_clone_on_cache_write_failed(
        ep, new_bdd, map_get, cache_write_branch, on_cache_write_failed);

    replicate_hdr_parsing_ops_on_cache_write_failed(
        ep, new_bdd, cache_write_branch, on_cache_write_failed);

    delete_coalescing_nodes_on_success(ep, new_bdd, on_cache_write_success,
                                       map_objs, fcfs_cached_table_data.key,
                                       deleted_branch_constraints);

    return new_bdd;
  }
};

} // namespace tofino
} // namespace synapse
