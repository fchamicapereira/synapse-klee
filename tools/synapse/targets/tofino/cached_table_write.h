#pragma once

#include "tofino_module.h"

#include "if.h"
#include "then.h"
#include "else.h"
#include "send_to_controller.h"

namespace synapse {
namespace tofino {

class CachedTableWrite : public TofinoModule {
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
  CachedTableWrite(const bdd::Node *node, DS_ID _cached_table_id,
                   const std::unordered_set<DS_ID> &_cached_table_byproducts,
                   addr_t _obj, klee::ref<klee::Expr> _key,
                   klee::ref<klee::Expr> _read_value,
                   klee::ref<klee::Expr> _write_value,
                   const symbol_t &_map_has_this_key,
                   const symbol_t &_cache_write_failed)
      : TofinoModule(ModuleType::Tofino_CachedTableWrite, "CachedTableWrite",
                     node),
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
    Module *cloned = new CachedTableWrite(
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

class CachedTableWriteGenerator : public TofinoModuleGenerator {
public:
  CachedTableWriteGenerator()
      : TofinoModuleGenerator(ModuleType::Tofino_CachedTableWrite,
                              "CachedTableWrite") {}

protected:
  virtual std::optional<speculation_t>
  speculate(const EP *ep, const bdd::Node *node,
            const Context &ctx) const override {
    if (node->get_type() != bdd::NodeType::CALL) {
      return std::nullopt;
    }

    const bdd::Call *dchain_allocate_new_index =
        static_cast<const bdd::Call *>(node);

    std::vector<const bdd::Call *> future_map_puts;
    if (!is_map_update_with_dchain(ep, dchain_allocate_new_index,
                                   future_map_puts)) {
      // The cached table read should deal with these cases.
      return std::nullopt;
    }

    map_coalescing_data_t coalescing_data;
    if (!get_map_coalescing_data_from_dchain_op(ep, dchain_allocate_new_index,
                                                coalescing_data)) {
      return std::nullopt;
    }

    if (!can_place_cached_table(ep, coalescing_data)) {
      return std::nullopt;
    }

    cached_table_data_t cached_table_data =
        get_cached_table_data(ep, future_map_puts);

    std::unordered_set<int> allowed_cache_capacities =
        enumerate_cache_table_capacities(cached_table_data.num_entries);

    double chosen_success_estimation = 0;
    int chosen_cache_capacity = 0;

    // We can use a different method for picking the right estimation depending
    // on the time it takes to find a solution.
    for (int cache_capacity : allowed_cache_capacities) {
      double success_estimation = get_cache_write_success_estimation_rel(
          ep, node, cache_capacity, cached_table_data.num_entries);

      if (success_estimation > chosen_success_estimation) {
        chosen_success_estimation = success_estimation;
        chosen_cache_capacity = cache_capacity;
      }
    }

    if (!can_get_or_build_cached_table(ep, node, cached_table_data,
                                       chosen_cache_capacity)) {
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
        get_nodes_to_speculatively_ignore(ep, dchain_allocate_new_index,
                                          coalescing_data,
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

    const bdd::Call *dchain_allocate_new_index =
        static_cast<const bdd::Call *>(node);

    std::vector<const bdd::Call *> future_map_puts;
    if (!is_map_update_with_dchain(ep, dchain_allocate_new_index,
                                   future_map_puts)) {
      // The cached table read should deal with these cases.
      return products;
    }

    map_coalescing_data_t coalescing_data;
    if (!get_map_coalescing_data_from_dchain_op(ep, dchain_allocate_new_index,
                                                coalescing_data)) {
      return products;
    }

    if (!can_place_cached_table(ep, coalescing_data)) {
      return products;
    }

    assert(false && "TODO");

    return products;
  }

private:
  double get_cache_write_success_estimation_rel(const EP *ep,
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

  cached_table_data_t
  get_cached_table_data(const EP *ep,
                        std::vector<const bdd::Call *> future_map_puts) const {
    cached_table_data_t cached_table_data;

    assert(!future_map_puts.empty());
    const bdd::Call *map_put = future_map_puts.front();

    const call_t &put_call = map_put->get_call();
    klee::ref<klee::Expr> obj_expr = put_call.args.at("map").expr;

    cached_table_data.obj = kutil::expr_addr_to_obj_addr(obj_expr);
    cached_table_data.key = put_call.args.at("key").in;
    cached_table_data.write_value = put_call.args.at("value").expr;

    const Context &ctx = ep->get_ctx();
    const bdd::map_config_t &cfg = ctx.get_map_config(cached_table_data.obj);

    cached_table_data.num_entries = cfg.capacity;
    return cached_table_data;
  }

  std::vector<const bdd::Node *> get_nodes_to_speculatively_ignore(
      const EP *ep, const bdd::Call *dchain_allocate_new_index,
      const map_coalescing_data_t &coalescing_data,
      klee::ref<klee::Expr> key) const {
    const bdd::BDD *bdd = ep->get_bdd();

    std::vector<const bdd::Node *> nodes_to_ignore =
        get_coalescing_nodes_from_key(bdd, dchain_allocate_new_index, key,
                                      coalescing_data);

    symbol_t out_of_space;
    bool found =
        get_symbol(dchain_allocate_new_index->get_locally_generated_symbols(),
                   "out_of_space", out_of_space);
    assert(found && "Symbol out_of_space not found");

    const bdd::Branch *branch = find_branch_checking_index_alloc(
        ep, dchain_allocate_new_index, out_of_space);

    if (branch) {
      nodes_to_ignore.push_back(branch);

      bool direction_to_keep = true;
      const bdd::Node *next =
          direction_to_keep ? branch->get_on_false() : branch->get_on_true();

      next->visit_nodes([&nodes_to_ignore](const bdd::Node *node) {
        nodes_to_ignore.push_back(node);
        return bdd::NodeVisitAction::VISIT_CHILDREN;
      });
    }

    return nodes_to_ignore;
  }
};

} // namespace tofino
} // namespace synapse
