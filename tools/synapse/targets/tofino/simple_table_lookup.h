#pragma once

#include "tofino_module.h"

namespace synapse {
namespace tofino {

class SimpleTableLookup : public TofinoModule {
private:
  DS_ID table_id;
  addr_t obj;
  std::vector<klee::ref<klee::Expr>> keys;
  std::vector<klee::ref<klee::Expr>> values;
  std::optional<symbol_t> hit;

public:
  SimpleTableLookup(const bdd::Node *node, DS_ID _table_id, addr_t _obj,
                    const std::vector<klee::ref<klee::Expr>> &_keys,
                    const std::vector<klee::ref<klee::Expr>> &_values,
                    const std::optional<symbol_t> &_hit)
      : TofinoModule(ModuleType::Tofino_SimpleTableLookup, "SimpleTableLookup",
                     node),
        table_id(_table_id), obj(_obj), keys(_keys), values(_values),
        hit(_hit) {}

  virtual void visit(EPVisitor &visitor, const EP *ep,
                     const EPNode *ep_node) const override {
    visitor.visit(ep, ep_node, this);
  }

  virtual Module *clone() const override {
    Module *cloned =
        new SimpleTableLookup(node, table_id, obj, keys, values, hit);
    return cloned;
  }

  DS_ID get_table_id() const { return table_id; }
  addr_t get_obj() const { return obj; }
  const std::vector<klee::ref<klee::Expr>> &get_keys() const { return keys; }
  const std::vector<klee::ref<klee::Expr>> &get_values() const {
    return values;
  }
  const std::optional<symbol_t> &get_hit() const { return hit; }

  virtual std::unordered_set<DS_ID> get_generated_ds() const override {
    return {table_id};
  }
};

class SimpleTableLookupGenerator : public TofinoModuleGenerator {
public:
  SimpleTableLookupGenerator()
      : TofinoModuleGenerator(ModuleType::Tofino_SimpleTableLookup,
                              "SimpleTableLookup") {}

protected:
  virtual std::optional<speculation_t>
  speculate(const EP *ep, const bdd::Node *node,
            const Context &ctx) const override {
    if (node->get_type() != bdd::NodeType::CALL) {
      return std::nullopt;
    }

    const bdd::Call *call_node = static_cast<const bdd::Call *>(node);

    if (!can_place_in_simple_table(ep, call_node)) {
      return std::nullopt;
    }

    addr_t obj;
    int num_entries;
    std::vector<klee::ref<klee::Expr>> keys;
    std::vector<klee::ref<klee::Expr>> values;
    std::optional<symbol_t> hit;
    DS_ID id;

    if (!get_table_data(ep, call_node, obj, num_entries, keys, values, hit,
                        id)) {
      return std::nullopt;
    }

    std::unordered_set<DS_ID> deps;
    Table *table =
        build_table(ep, node, id, num_entries, keys, values, hit, deps);

    if (!table) {
      return std::nullopt;
    }

    delete table;

    return ctx;
  }

  virtual std::vector<__generator_product_t>
  process_node(const EP *ep, const bdd::Node *node) const override {
    std::vector<__generator_product_t> products;

    if (node->get_type() != bdd::NodeType::CALL) {
      return products;
    }

    const bdd::Call *call_node = static_cast<const bdd::Call *>(node);

    if (!can_place_in_simple_table(ep, call_node)) {
      return products;
    }

    addr_t obj;
    int num_entries;
    std::vector<klee::ref<klee::Expr>> keys;
    std::vector<klee::ref<klee::Expr>> values;
    std::optional<symbol_t> hit;
    DS_ID id;

    if (!get_table_data(ep, call_node, obj, num_entries, keys, values, hit,
                        id)) {
      return products;
    }

    std::unordered_set<DS_ID> deps;
    Table *table =
        build_table(ep, node, id, num_entries, keys, values, hit, deps);

    if (!table) {
      return products;
    }

    Module *module = new SimpleTableLookup(node, id, obj, keys, values, hit);
    EPNode *ep_node = new EPNode(module);

    EP *new_ep = new EP(*ep);
    products.emplace_back(new_ep);

    place_simple_table(new_ep, obj, table, deps);

    EPLeaf leaf(ep_node, node->get_next());
    new_ep->process_leaf(ep_node, {leaf});

    return products;
  }

private:
  Table *build_table(const EP *ep, const bdd::Node *node, DS_ID id,
                     int num_entries,
                     const std::vector<klee::ref<klee::Expr>> &keys,
                     const std::vector<klee::ref<klee::Expr>> &values,
                     const std::optional<symbol_t> &hit,
                     std::unordered_set<DS_ID> &deps) const {
    std::vector<bits_t> keys_size;
    for (klee::ref<klee::Expr> key : keys) {
      keys_size.push_back(key->getWidth());
    }

    std::vector<bits_t> params_size;
    for (klee::ref<klee::Expr> value : values) {
      params_size.push_back(value->getWidth());
    }

    Table *table = new Table(id, num_entries, keys_size, params_size);

    const TofinoContext *tofino_ctx = get_tofino_ctx(ep);
    deps = tofino_ctx->get_stateful_deps(ep, node);

    if (!tofino_ctx->check_placement(ep, table, deps)) {
      delete table;
      return nullptr;
    }

    return table;
  }

  void place_simple_table(EP *ep, addr_t obj, Table *table,
                          const std::unordered_set<DS_ID> &deps) const {
    TofinoContext *tofino_ctx = get_mutable_tofino_ctx(ep);
    place(ep, obj, PlacementDecision::Tofino_SimpleTable);
    tofino_ctx->place(ep, obj, table, deps);

    Log::dbg() << "-> ~~~ NEW PLACEMENT ~~~ <-\n";
    table->log_debug();
    tofino_ctx->get_tna().log_debug_placement();
  }

  bool can_place_in_simple_table(const EP *ep,
                                 const bdd::Call *call_node) const {
    const call_t &call = call_node->get_call();

    std::string obj_arg;
    if (call.function_name == "map_get") {
      obj_arg = "map";
    } else if (call.function_name == "vector_borrow") {
      obj_arg = "vector";
    } else if (call.function_name == "dchain_is_index_allocated" ||
               call.function_name == "dchain_rejuvenate_index") {
      obj_arg = "chain";
    } else {
      return false;
    }

    return can_place(ep, call_node, obj_arg,
                     PlacementDecision::Tofino_SimpleTable);
  }

  bool get_table_data(const EP *ep, const bdd::Call *call_node, addr_t &obj,
                      int &num_entries,
                      std::vector<klee::ref<klee::Expr>> &keys,
                      std::vector<klee::ref<klee::Expr>> &values,
                      std::optional<symbol_t> &hit, DS_ID &id) const {
    const call_t &call = call_node->get_call();

    if (call.function_name == "map_get") {
      return table_data_from_map_op(ep, call_node, obj, num_entries, keys,
                                    values, hit, id);
    }

    if (call.function_name == "vector_borrow") {
      return table_data_from_vector_op(ep, call_node, obj, num_entries, keys,
                                       values, hit, id);
    }

    if (call.function_name == "dchain_is_index_allocated" ||
        call.function_name == "dchain_rejuvenate_index") {
      return table_data_from_dchain_op(ep, call_node, obj, num_entries, keys,
                                       values, hit, id);
    }

    return false;
  }

  bool table_data_from_map_op(const EP *ep, const bdd::Call *call_node,
                              addr_t &obj, int &num_entries,
                              std::vector<klee::ref<klee::Expr>> &keys,
                              std::vector<klee::ref<klee::Expr>> &values,
                              std::optional<symbol_t> &hit, DS_ID &id) const {
    const call_t &call = call_node->get_call();
    assert(call.function_name == "map_get");

    klee::ref<klee::Expr> map_addr_expr = call.args.at("map").expr;
    klee::ref<klee::Expr> key = call.args.at("key").in;
    klee::ref<klee::Expr> value_out = call.args.at("value_out").out;

    symbols_t symbols = call_node->get_locally_generated_symbols();

    symbol_t map_has_this_key;
    bool found = get_symbol(symbols, "map_has_this_key", map_has_this_key);
    assert(found && "Symbol map_has_this_key not found");

    obj = kutil::expr_addr_to_obj_addr(map_addr_expr);
    keys = Table::build_keys(key);
    values = {value_out};
    hit = map_has_this_key;
    id = "map_" + std::to_string(call_node->get_id());

    const Context &ctx = ep->get_ctx();
    const bdd::map_config_t &cfg = ctx.get_map_config(obj);
    num_entries = cfg.capacity;

    return true;
  }

  bool table_data_from_vector_op(const EP *ep, const bdd::Call *call_node,
                                 addr_t &obj, int &num_entries,
                                 std::vector<klee::ref<klee::Expr>> &keys,
                                 std::vector<klee::ref<klee::Expr>> &values,
                                 std::optional<symbol_t> &hit,
                                 DS_ID &id) const {
    if (!is_vector_read(call_node)) {
      return false;
    }

    const call_t &call = call_node->get_call();
    assert(call.function_name == "vector_borrow");

    klee::ref<klee::Expr> vector_addr_expr = call.args.at("vector").expr;
    klee::ref<klee::Expr> index = call.args.at("index").expr;
    klee::ref<klee::Expr> cell = call.extra_vars.at("borrowed_cell").second;

    obj = kutil::expr_addr_to_obj_addr(vector_addr_expr);
    keys = {index};
    values = {cell};
    id = "vector_" + std::to_string(call_node->get_id());

    const Context &ctx = ep->get_ctx();
    const bdd::vector_config_t &cfg = ctx.get_vector_config(obj);
    num_entries = cfg.capacity;

    return true;
  }

  bool table_data_from_dchain_op(const EP *ep, const bdd::Call *call_node,
                                 addr_t &obj, int &num_entries,
                                 std::vector<klee::ref<klee::Expr>> &keys,
                                 std::vector<klee::ref<klee::Expr>> &values,
                                 std::optional<symbol_t> &hit,
                                 DS_ID &id) const {
    const call_t &call = call_node->get_call();
    assert(call.function_name == "dchain_is_index_allocated" ||
           call.function_name == "dchain_rejuvenate_index");

    klee::ref<klee::Expr> dchain_addr_expr = call.args.at("chain").expr;
    klee::ref<klee::Expr> index = call.args.at("index").expr;

    addr_t dchain_addr = kutil::expr_addr_to_obj_addr(dchain_addr_expr);

    obj = dchain_addr;
    keys = {index};

    if (call.function_name == "dchain_is_index_allocated") {
      symbols_t symbols = call_node->get_locally_generated_symbols();
      symbol_t is_allocated;
      bool found =
          get_symbol(symbols, "dchain_is_index_allocated", is_allocated);
      assert(found && "Symbol dchain_is_index_allocated not found");

      hit = is_allocated;
    }

    id = "dchain_" + std::to_string(call_node->get_id());

    const Context &ctx = ep->get_ctx();
    const bdd::dchain_config_t &cfg = ctx.get_dchain_config(obj);
    num_entries = cfg.index_range;

    return true;
  }
};

} // namespace tofino
} // namespace synapse
