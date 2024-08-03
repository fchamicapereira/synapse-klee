#pragma once

#include "tofino_module.h"

namespace synapse {
namespace tofino {

class TTLCachedTableRead : public TofinoModule {
private:
  DS_ID cached_table_id;
  std::unordered_set<DS_ID> cached_table_byproducts;

  addr_t obj;
  klee::ref<klee::Expr> key;
  klee::ref<klee::Expr> value;
  symbol_t map_has_this_key;

public:
  TTLCachedTableRead(const bdd::Node *node, DS_ID _cached_table_id,
                     const std::unordered_set<DS_ID> &_cached_table_byproducts,
                     addr_t _obj, klee::ref<klee::Expr> _key,
                     klee::ref<klee::Expr> _value,
                     const symbol_t &_map_has_this_key)
      : TofinoModule(ModuleType::Tofino_TTLCachedTableRead,
                     "TTLCachedTableRead", node),
        cached_table_id(_cached_table_id),
        cached_table_byproducts(_cached_table_byproducts), obj(_obj), key(_key),
        value(_value), map_has_this_key(_map_has_this_key) {}

  virtual void visit(EPVisitor &visitor, const EP *ep,
                     const EPNode *ep_node) const override {
    visitor.visit(ep, ep_node, this);
  }

  virtual Module *clone() const override {
    Module *cloned =
        new TTLCachedTableRead(node, cached_table_id, cached_table_byproducts,
                               obj, key, value, map_has_this_key);
    return cloned;
  }

  DS_ID get_id() const { return cached_table_id; }
  addr_t get_obj() const { return obj; }
  klee::ref<klee::Expr> get_key() const { return key; }
  klee::ref<klee::Expr> get_value() const { return value; }
  const symbol_t &get_map_has_this_key() const { return map_has_this_key; }

  virtual std::unordered_set<DS_ID> get_generated_ds() const override {
    return cached_table_byproducts;
  }
};

class TTLCachedTableReadGenerator : public TofinoModuleGenerator {
public:
  TTLCachedTableReadGenerator()
      : TofinoModuleGenerator(ModuleType::Tofino_TTLCachedTableRead,
                              "TTLCachedTableRead") {}

protected:
  virtual std::optional<speculation_t>
  speculate(const EP *ep, const bdd::Node *node,
            const Context &ctx) const override {
    if (node->get_type() != bdd::NodeType::CALL) {
      return std::nullopt;
    }

    const bdd::Call *map_get = static_cast<const bdd::Call *>(node);
    const call_t &call = map_get->get_call();

    if (call.function_name != "map_get") {
      return std::nullopt;
    }

    map_coalescing_data_t coalescing_data;
    if (!get_map_coalescing_data_from_map_op(ep, map_get, coalescing_data)) {
      return std::nullopt;
    }

    if (!can_place_cached_table(ep, coalescing_data)) {
      return std::nullopt;
    }

    cached_table_data_t cached_table_data = get_cached_table_data(ep, map_get);

    std::unordered_set<DS_ID> deps;
    if (!get_cached_table(ep, node, cached_table_data, deps)) {
      return std::nullopt;
    }

    std::vector<const bdd::Call *> vector_ops =
        get_future_vector_key_ops(ep, node, cached_table_data, coalescing_data);

    Context new_ctx = ctx;
    speculation_t speculation(new_ctx);
    for (const bdd::Call *vector_op : vector_ops) {
      speculation.skip.insert(vector_op->get_id());
    }

    return ctx;
  }

  virtual std::vector<__generator_product_t>
  process_node(const EP *ep, const bdd::Node *node) const override {
    std::vector<__generator_product_t> products;

    if (node->get_type() != bdd::NodeType::CALL) {
      return products;
    }

    const bdd::Call *map_get = static_cast<const bdd::Call *>(node);
    const call_t &call = map_get->get_call();

    if (call.function_name != "map_get") {
      return products;
    }

    map_coalescing_data_t coalescing_data;
    if (!get_map_coalescing_data_from_map_op(ep, map_get, coalescing_data)) {
      return products;
    }

    if (!can_place_cached_table(ep, coalescing_data)) {
      return products;
    }

    cached_table_data_t cached_table_data = get_cached_table_data(ep, map_get);

    std::unordered_set<int> allowed_cache_capacities =
        enumerate_cache_table_capacities(cached_table_data.num_entries);

    for (int cache_capacity : allowed_cache_capacities) {
      std::optional<__generator_product_t> product =
          concretize_cached_table_read(ep, node, coalescing_data,
                                       cached_table_data, cache_capacity);

      if (product.has_value()) {
        products.push_back(*product);
      }
    }

    return products;
  }

private:
  std::optional<__generator_product_t>
  concretize_cached_table_read(const EP *ep, const bdd::Node *node,
                               const map_coalescing_data_t &coalescing_data,
                               const cached_table_data_t &cached_table_data,
                               int cache_capacity) const {
    std::unordered_set<DS_ID> deps;
    bool already_exists;

    TTLCachedTable *cached_table = get_or_build_cached_table(
        ep, node, cached_table_data, cache_capacity, already_exists, deps);

    if (!cached_table) {
      return std::nullopt;
    }

    std::unordered_set<DS_ID> byproducts =
        get_cached_table_byproducts(cached_table);

    Module *module = new TTLCachedTableRead(
        node, cached_table->id, byproducts, cached_table_data.obj,
        cached_table_data.key, cached_table_data.read_value,
        cached_table_data.map_has_this_key);
    EPNode *ep_node = new EPNode(module);

    EP *new_ep = new EP(*ep);

    const bdd::Node *new_next;
    bdd::BDD *bdd = delete_future_vector_key_ops(
        new_ep, node, cached_table_data, coalescing_data, new_next);

    if (!already_exists) {
      place_cached_table(new_ep, coalescing_data, cached_table, deps);
    }

    EPLeaf leaf(ep_node, new_next);
    new_ep->process_leaf(ep_node, {leaf});
    new_ep->replace_bdd(bdd);

    new_ep->inspect();

    std::stringstream descr;
    descr << "cap=" << cached_table->cache_capacity;

    return __generator_product_t(new_ep, descr.str());
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

  cached_table_data_t get_cached_table_data(const EP *ep,
                                            const bdd::Call *map_get) const {
    cached_table_data_t cached_table_data;

    const call_t &call = map_get->get_call();
    symbols_t symbols = map_get->get_locally_generated_symbols();
    klee::ref<klee::Expr> obj_expr = call.args.at("map").expr;

    cached_table_data.obj = kutil::expr_addr_to_obj_addr(obj_expr);
    cached_table_data.key = call.args.at("key").in;
    cached_table_data.read_value = call.args.at("value_out").out;

    bool found = get_symbol(symbols, "map_has_this_key",
                            cached_table_data.map_has_this_key);
    assert(found && "Symbol map_has_this_key not found");

    const Context &ctx = ep->get_ctx();
    const bdd::map_config_t &cfg = ctx.get_map_config(cached_table_data.obj);

    cached_table_data.num_entries = cfg.capacity;

    return cached_table_data;
  }

  std::vector<const bdd::Call *> get_future_vector_key_ops(
      const EP *ep, const bdd::Node *node,
      const cached_table_data_t &cached_table_data,
      const map_coalescing_data_t &coalescing_data) const {
    std::vector<const bdd::Call *> vector_ops =
        get_future_functions(node, {"vector_borrow", "vector_return"});

    for (const bdd::Call *vector_op : vector_ops) {
      const call_t &call = vector_op->get_call();

      klee::ref<klee::Expr> vector = call.args.at("vector").expr;
      klee::ref<klee::Expr> index = call.args.at("index").expr;

      addr_t vector_addr = kutil::expr_addr_to_obj_addr(vector);

      if (vector_addr != coalescing_data.vector_key) {
        continue;
      }

      if (!kutil::solver_toolbox.are_exprs_always_equal(
              index, cached_table_data.read_value)) {
        continue;
      }

      vector_ops.push_back(vector_op);
    }

    return vector_ops;
  }

  bdd::BDD *
  delete_future_vector_key_ops(EP *ep, const bdd::Node *node,
                               const cached_table_data_t &cached_table_data,
                               const map_coalescing_data_t &coalescing_data,
                               const bdd::Node *&new_next) const {
    const bdd::BDD *old_bdd = ep->get_bdd();
    bdd::BDD *new_bdd = new bdd::BDD(*old_bdd);

    const bdd::Node *next = node->get_next();

    if (next) {
      new_next = new_bdd->get_node_by_id(next->get_id());
    } else {
      new_next = nullptr;
    }

    std::vector<const bdd::Call *> vector_ops =
        get_future_vector_key_ops(ep, node, cached_table_data, coalescing_data);

    for (const bdd::Call *vector_op : vector_ops) {
      bool replace_next = (vector_op == next);
      bdd::Node *replacement;
      delete_non_branch_node_from_bdd(ep, new_bdd, vector_op->get_id(),
                                      replacement);

      if (replace_next) {
        new_next = replacement;
      }
    }

    return new_bdd;
  }
};

} // namespace tofino
} // namespace synapse
