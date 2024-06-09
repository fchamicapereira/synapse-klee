#pragma once

#include "tofino_module.h"

namespace synapse {
namespace tofino {

class CachedTableDelete : public TofinoModule {
private:
  DS_ID cached_table_id;
  std::unordered_set<DS_ID> cached_table_byproducts;

  addr_t obj;
  klee::ref<klee::Expr> key;
  klee::ref<klee::Expr> value;
  symbol_t map_has_this_key;

public:
  CachedTableDelete(const bdd::Node *node, DS_ID _cached_table_id,
                    const std::unordered_set<DS_ID> &_cached_table_byproducts,
                    addr_t _obj, klee::ref<klee::Expr> _key,
                    klee::ref<klee::Expr> _value,
                    const symbol_t &_map_has_this_key)
      : TofinoModule(ModuleType::Tofino_CachedTableDelete, "CachedTableDelete",
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
        new CachedTableDelete(node, cached_table_id, cached_table_byproducts,
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

class CachedTableDeleteGenerator : public TofinoModuleGenerator {
public:
  CachedTableDeleteGenerator()
      : TofinoModuleGenerator(ModuleType::Tofino_CachedTableDelete,
                              "CachedTableDelete") {}

protected:
  virtual std::vector<const EP *>
  process_node(const EP *ep, const bdd::Node *node) const override {
    std::vector<const EP *> new_eps;

    if (node->get_type() != bdd::NodeType::CALL) {
      return new_eps;
    }

    const bdd::Call *call_node = static_cast<const bdd::Call *>(node);
    const call_t &call = call_node->get_call();

    if (call.function_name != "map_erase" &&
        call.function_name != "dchain_free_index") {
      return new_eps;
    }

    const bdd::Call *map_erase;

    if (call.function_name == "dchain_free_index") {
      map_erase = get_map_erase_after_dchain_free(ep, call_node);
    } else {
      map_erase = call_node;
    }

    if (!map_erase) {
      return new_eps;
    }

    // A bit too strict, but I don't want to build the cached table ds in this
    // module because we don't have a value.
    if (!check_placement(ep, map_erase, "map",
                         PlacementDecision::Tofino_CachedTable)) {
      return new_eps;
    }

    cached_table_data_t data = get_cached_table_data(ep, map_erase);

    std::unordered_set<DS_ID> deps;
    CachedTable *cached_table = get_cached_table(ep, data, deps);

    if (!cached_table) {
      return new_eps;
    }

    std::unordered_set<DS_ID> byproducts =
        get_cached_table_byproducts(cached_table);

    Module *module =
        new CachedTableDelete(node, cached_table->id, byproducts, data.obj,
                              data.key, data.read_value, data.map_has_this_key);
    EPNode *ep_node = new EPNode(module);

    EP *new_ep = new EP(*ep);
    new_eps.push_back(new_ep);

    const bdd::Node *new_next;
    bdd::BDD *bdd =
        delete_future_related_nodes(new_ep, node, data.obj, new_next);

    EPLeaf leaf(ep_node, new_next);
    new_ep->process_leaf(ep_node, {leaf});
    new_ep->replace_bdd(bdd);

    new_ep->inspect();

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

  const bdd::Call *
  get_map_erase_after_dchain_free(const EP *ep,
                                  const bdd::Call *dchain_free_index) const {
    const call_t &dchain_free_index_call = dchain_free_index->get_call();
    assert(dchain_free_index_call.function_name == "dchain_free_index");

    klee::ref<klee::Expr> chain_addr_expr =
        dchain_free_index_call.args.at("chain").expr;
    addr_t chain_obj = kutil::expr_addr_to_obj_addr(chain_addr_expr);

    const Context &ctx = ep->get_ctx();
    std::optional<map_coalescing_data_t> data =
        ctx.get_coalescing_data(chain_obj);

    if (!data.has_value()) {
      return nullptr;
    }

    std::vector<const bdd::Node *> ops =
        get_future_functions(dchain_free_index, {"map_erase"});

    if (ops.empty()) {
      return nullptr;
    }

    for (const bdd::Node *op : ops) {
      const bdd::Call *map_erase = static_cast<const bdd::Call *>(op);
      const call_t &map_erase_call = map_erase->get_call();

      klee::ref<klee::Expr> map_addr_expr = map_erase_call.args.at("map").expr;
      addr_t map_obj = kutil::expr_addr_to_obj_addr(map_addr_expr);

      if (map_obj == data->map) {
        return map_erase;
      }
    }

    return nullptr;
  }

  cached_table_data_t get_cached_table_data(const EP *ep,
                                            const bdd::Call *map_erase) const {
    cached_table_data_t data;

    const call_t &call = map_erase->get_call();
    assert(call.function_name == "map_erase");

    klee::ref<klee::Expr> map_addr_expr = call.args.at("map").expr;
    klee::ref<klee::Expr> key = call.args.at("key").in;

    data.obj = kutil::expr_addr_to_obj_addr(map_addr_expr);
    data.key = key;

    const Context &ctx = ep->get_ctx();
    const bdd::map_config_t &cfg = ctx.get_map_config(data.obj);

    data.num_entries = cfg.capacity;

    return data;
  }

  bdd::BDD *delete_future_related_nodes(const EP *ep, const bdd::Node *node,
                                        addr_t map_obj,
                                        const bdd::Node *&new_next) const {
    const bdd::BDD *old_bdd = ep->get_bdd();
    bdd::BDD *new_bdd = new bdd::BDD(*old_bdd);

    const bdd::Node *next = node->get_next();
    new_next = next;

    const Context &ctx = ep->get_ctx();
    std::optional<map_coalescing_data_t> data =
        ctx.get_coalescing_data(map_obj);
    assert(data.has_value());

    std::vector<const bdd::Node *> ops =
        get_future_functions(node, {"vector_borrow", "vector_return",
                                    "dchain_free_index", "map_put"});

    for (const bdd::Node *op : ops) {
      assert(op->get_type() == bdd::NodeType::CALL);

      const bdd::Call *call_node = static_cast<const bdd::Call *>(op);
      const call_t &call = call_node->get_call();

      if (call.function_name == "dchain_free_index") {
        klee::ref<klee::Expr> obj_expr = call.args.at("chain").expr;
        addr_t obj = kutil::expr_addr_to_obj_addr(obj_expr);

        if (obj != data->dchain) {
          continue;
        }
      } else if (call.function_name == "map_put") {
        klee::ref<klee::Expr> obj_expr = call.args.at("map").expr;
        addr_t obj = kutil::expr_addr_to_obj_addr(obj_expr);

        if (obj != data->map) {
          continue;
        }
      } else {
        klee::ref<klee::Expr> obj_expr = call.args.at("vector").expr;
        addr_t obj = kutil::expr_addr_to_obj_addr(obj_expr);

        if (obj != data->vector_key) {
          continue;
        }
      }

      bool replace_next = (op == next);
      bdd::Node *replacement;
      delete_non_branch_node_from_bdd(ep, new_bdd, op, replacement);

      if (replace_next) {
        new_next = replacement;
      }
    }

    return new_bdd;
  }
};

} // namespace tofino
} // namespace synapse
