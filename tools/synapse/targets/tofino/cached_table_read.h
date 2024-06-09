#pragma once

#include "tofino_module.h"

namespace synapse {
namespace tofino {

class CachedTableRead : public TofinoModule {
private:
  DS_ID cached_table_id;
  std::unordered_set<DS_ID> cached_table_byproducts;

  addr_t obj;
  klee::ref<klee::Expr> key;
  klee::ref<klee::Expr> value;
  symbol_t map_has_this_key;

public:
  CachedTableRead(const bdd::Node *node, DS_ID _cached_table_id,
                  const std::unordered_set<DS_ID> &_cached_table_byproducts,
                  addr_t _obj, klee::ref<klee::Expr> _key,
                  klee::ref<klee::Expr> _value,
                  const symbol_t &_map_has_this_key)
      : TofinoModule(ModuleType::Tofino_CachedTableRead, "CachedTableRead",
                     node),
        cached_table_id(_cached_table_id),
        cached_table_byproducts(_cached_table_byproducts), obj(_obj), key(_key),
        value(_value), map_has_this_key(_map_has_this_key) {}

  virtual void visit(EPVisitor &visitor, const EP *ep,
                     const EPNode *ep_node) const override {
    visitor.visit(ep, ep_node, this);
  }

  virtual Module *clone() const override {
    Module *cloned =
        new CachedTableRead(node, cached_table_id, cached_table_byproducts, obj,
                            key, value, map_has_this_key);
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

class CachedTableReadGenerator : public TofinoModuleGenerator {
public:
  CachedTableReadGenerator()
      : TofinoModuleGenerator(ModuleType::Tofino_CachedTableRead,
                              "CachedTableRead") {}

protected:
  virtual std::vector<const EP *>
  process_node(const EP *ep, const bdd::Node *node) const override {
    std::vector<const EP *> new_eps;

    if (node->get_type() != bdd::NodeType::CALL) {
      return new_eps;
    }

    const bdd::Call *call_node = static_cast<const bdd::Call *>(node);
    const call_t &call = call_node->get_call();

    if (call.function_name != "map_get") {
      return new_eps;
    }

    map_coalescing_data_t coalescing_data;
    if (!can_place_cached_table(ep, call_node, coalescing_data)) {
      return new_eps;
    }

    std::vector<const bdd::Call *> future_map_puts;
    if (is_map_get_followed_by_map_puts_on_miss(ep->get_bdd(), call_node,
                                                future_map_puts)) {
      // The cached table conditional write should deal with these cases.
      return new_eps;
    }

    cached_table_data_t data = get_cached_table_data(ep, call_node);

    std::unordered_set<DS_ID> deps;
    int cache_capacity = 1024;
    bool already_exists;
    CachedTable *cached_table = get_or_build_cached_table(
        ep, node, data, cache_capacity, already_exists, deps);

    if (!cached_table) {
      return new_eps;
    }

    std::unordered_set<DS_ID> byproducts =
        get_cached_table_byproducts(cached_table);

    Module *module =
        new CachedTableRead(node, cached_table->id, byproducts, data.obj,
                            data.key, data.read_value, data.map_has_this_key);
    EPNode *ep_node = new EPNode(module);

    EP *new_ep = new EP(*ep);
    new_eps.push_back(new_ep);

    const bdd::Node *new_next;
    bdd::BDD *bdd = delete_future_vector_key_ops(new_ep, node, data,
                                                 coalescing_data, new_next);

    EPLeaf leaf(ep_node, new_next);
    new_ep->process_leaf(ep_node, {leaf});
    new_ep->replace_bdd(bdd);

    new_ep->inspect();

    if (!already_exists) {
      place_cached_table(new_ep, coalescing_data, cached_table, deps);
    }

    return new_eps;
  }

private:
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
    cached_table_data_t data;

    const call_t &call = map_get->get_call();
    symbols_t symbols = map_get->get_locally_generated_symbols();
    klee::ref<klee::Expr> obj_expr = call.args.at("map").expr;

    data.obj = kutil::expr_addr_to_obj_addr(obj_expr);
    data.key = call.args.at("key").in;
    data.read_value = call.args.at("value_out").out;

    bool found = get_symbol(symbols, "map_has_this_key", data.map_has_this_key);
    assert(found && "Symbol map_has_this_key not found");

    const Context &ctx = ep->get_ctx();
    const bdd::map_config_t &cfg = ctx.get_map_config(data.obj);

    data.num_entries = cfg.capacity;

    return data;
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

    std::vector<const bdd::Node *> vector_ops =
        get_future_functions(node, {"vector_borrow", "vector_return"});

    for (const bdd::Node *vector_op : vector_ops) {
      assert(vector_op->get_type() == bdd::NodeType::CALL);

      const bdd::Call *vector_call = static_cast<const bdd::Call *>(vector_op);
      const call_t &call = vector_call->get_call();

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

      bool replace_next = (vector_op == next);
      bdd::Node *replacement;
      delete_non_branch_node_from_bdd(ep, new_bdd, vector_op, replacement);

      if (replace_next) {
        new_next = replacement;
      }
    }

    return new_bdd;
  }
};

} // namespace tofino
} // namespace synapse
