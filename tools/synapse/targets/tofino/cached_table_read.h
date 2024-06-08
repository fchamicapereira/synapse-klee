#pragma once

#include "tofino_module.h"

namespace synapse {
namespace tofino {

class CachedTableRead : public TofinoModule {
private:
  DS_ID id;
  addr_t obj;
  klee::ref<klee::Expr> key;
  klee::ref<klee::Expr> value;
  symbol_t map_has_this_key;

public:
  CachedTableRead(const bdd::Node *node, DS_ID _id, addr_t _obj,
                  klee::ref<klee::Expr> _key, klee::ref<klee::Expr> _value,
                  const symbol_t &_map_has_this_key)
      : TofinoModule(ModuleType::Tofino_CachedTableRead, "CachedTableRead",
                     node),
        id(_id), obj(_obj), key(_key), value(_value),
        map_has_this_key(_map_has_this_key) {}

  virtual void visit(EPVisitor &visitor, const EP *ep,
                     const EPNode *ep_node) const override {
    visitor.visit(ep, ep_node, this);
  }

  virtual Module *clone() const override {
    Module *cloned =
        new CachedTableRead(node, id, obj, key, value, map_has_this_key);
    return cloned;
  }

  DS_ID get_id() const { return id; }
  addr_t get_obj() const { return obj; }
  klee::ref<klee::Expr> get_key() const { return key; }
  klee::ref<klee::Expr> get_value() const { return value; }
  const symbol_t &get_map_has_this_key() const { return map_has_this_key; }

  virtual std::unordered_set<DS_ID> get_generated_ds() const override {
    return {id};
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
    DS *cached_table = get_or_build_cached_table(ep, node, data, cache_capacity,
                                                 already_exists, deps);

    if (!cached_table) {
      return new_eps;
    }

    Module *module =
        new CachedTableRead(node, cached_table->id, data.obj, data.key,
                            data.read_value, data.map_has_this_key);
    EPNode *ep_node = new EPNode(module);

    EP *new_ep = new EP(*ep);
    new_eps.push_back(new_ep);

    EPLeaf leaf(ep_node, node->get_next());
    new_ep->process_leaf(ep_node, {leaf});

    mark_future_vector_key_ops_as_processed(new_ep, node, coalescing_data);

    if (!already_exists) {
      place_cached_table(new_ep, coalescing_data, cached_table, deps);
    }

    return new_eps;
  }

private:
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

  void mark_future_vector_key_ops_as_processed(
      EP *ep, const bdd::Node *node,
      const map_coalescing_data_t &coalescing_data) const {
    std::vector<const bdd::Node *> vector_ops =
        get_all_functions_after_node(node, {"vector_borrow", "vector_return"});

    for (const bdd::Node *vector_op : vector_ops) {
      assert(vector_op->get_type() == bdd::NodeType::CALL);

      const bdd::Call *vector_call = static_cast<const bdd::Call *>(vector_op);
      const call_t &call = vector_call->get_call();

      klee::ref<klee::Expr> vector = call.args.at("vector").expr;
      addr_t vector_addr = kutil::expr_addr_to_obj_addr(vector);

      if (vector_addr != coalescing_data.vector_key) {
        continue;
      }

      ep->process_future_non_branch_node(vector_op);
    }
  }
};

} // namespace tofino
} // namespace synapse
