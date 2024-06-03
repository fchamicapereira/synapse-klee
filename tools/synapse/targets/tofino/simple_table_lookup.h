#pragma once

#include "tofino_module.h"

namespace synapse {
namespace tofino {

class SimpleTableLookup : public TofinoModule {
private:
  int table_id;
  addr_t obj;

public:
  SimpleTableLookup(const bdd::Node *node, int _table_id, addr_t _obj)
      : TofinoModule(ModuleType::Tofino_SimpleTableLookup, "SimpleTableLookup",
                     node),
        table_id(_table_id), obj(_obj) {}

  virtual void visit(EPVisitor &visitor, const EP *ep,
                     const EPNode *ep_node) const override {
    visitor.visit(ep, ep_node, this);
  }

  virtual Module *clone() const override {
    Module *cloned = new SimpleTableLookup(node, table_id, obj);
    return cloned;
  }

  int get_table_id() const { return table_id; }
  addr_t get_obj() const { return obj; }
};

class SimpleTableLookupGenerator : public TofinoModuleGenerator {
public:
  SimpleTableLookupGenerator()
      : TofinoModuleGenerator(ModuleType::Tofino_SimpleTableLookup,
                              "SimpleTableLookup") {}

protected:
  virtual std::vector<const EP *>
  process_node(const EP *ep, const bdd::Node *node) const override {
    std::vector<const EP *> new_eps;

    if (node->get_type() != bdd::NodeType::CALL) {
      return new_eps;
    }

    const bdd::Call *call_node = static_cast<const bdd::Call *>(node);

    if (!can_place_in_simple_table(ep, call_node)) {
      return new_eps;
    }

    addr_t obj;
    std::vector<klee::ref<klee::Expr>> keys;
    klee::ref<klee::Expr> value;
    std::optional<symbol_t> hit;

    if (!table_data_from_call(call_node, obj, keys, value, hit)) {
      return new_eps;
    }

    DataStructureID table_id = static_cast<DataStructureID>(node->get_id());
    Table *table = new Table(table_id, 0, keys, {value}, hit);

    Module *module = new SimpleTableLookup(node, table_id, obj);
    EPNode *ep_node = new EPNode(module);

    EP *new_ep = new EP(*ep);
    new_eps.push_back(new_ep);

    EPLeaf leaf(ep_node, node->get_next());
    new_ep->process_leaf(ep_node, {leaf});

    place(new_ep, obj, PlacementDecision::TofinoSimpleTable);

    TofinoContext *tofino_ctx = get_mutable_tofino_ctx(new_ep);
    tofino_ctx->add_data_structure(obj, table);

    return new_eps;
  }

private:
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
                     PlacementDecision::TofinoSimpleTable);
  }

  std::vector<klee::ref<klee::Expr>>
  build_keys(klee::ref<klee::Expr> key) const {
    std::vector<klee::ref<klee::Expr>> keys;

    std::vector<kutil::expr_group_t> groups = kutil::get_expr_groups(key);
    for (const kutil::expr_group_t &group : groups) {
      keys.push_back(group.expr);
    }

    return keys;
  }

  bool table_data_from_call(const bdd::Call *call_node, addr_t &obj,
                            std::vector<klee::ref<klee::Expr>> &keys,
                            klee::ref<klee::Expr> &value,
                            std::optional<symbol_t> &hit) const {
    const call_t &call = call_node->get_call();

    if (call.function_name == "map_get") {
      return table_data_from_map_op(call_node, obj, keys, value, hit);
    }

    if (call.function_name == "vector_borrow") {
      return table_data_from_vector_op(call_node, obj, keys, value, hit);
    }

    if (call.function_name == "dchain_is_index_allocated" ||
        call.function_name == "dchain_rejuvenate_index") {
      return table_data_from_dchain_op(call_node, obj, keys, value, hit);
    }

    return false;
  }

  bool table_data_from_map_op(const bdd::Call *call_node, addr_t &obj,
                              std::vector<klee::ref<klee::Expr>> &keys,
                              klee::ref<klee::Expr> &value,
                              std::optional<symbol_t> &hit) const {
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
    keys = build_keys(key);
    value = value_out;
    hit = map_has_this_key;

    return true;
  }

  bool is_vector_read(const bdd::Call *vector_borrow) const {
    const call_t &vb = vector_borrow->get_call();
    assert(vb.function_name == "vector_borrow");

    klee::ref<klee::Expr> vb_obj_expr = vb.args.at("vector").expr;
    klee::ref<klee::Expr> vb_index = vb.args.at("index").in;
    klee::ref<klee::Expr> vb_value = vb.extra_vars.at("borrowed_cell").second;

    addr_t vb_obj = kutil::expr_addr_to_obj_addr(vb_obj_expr);

    const bdd::Node *vector_return =
        get_future_vector_return(vector_borrow, vb_obj);
    assert(vector_return && "Vector return not found");
    assert(vector_return->get_type() == bdd::NodeType::CALL);

    const bdd::Call *vr_call = static_cast<const bdd::Call *>(vector_return);
    const call_t &vr = vr_call->get_call();
    assert(vr.function_name == "vector_return");

    klee::ref<klee::Expr> vr_obj_expr = vr.args.at("vector").expr;
    klee::ref<klee::Expr> vr_index = vr.args.at("index").in;
    klee::ref<klee::Expr> vr_value = vr.args.at("value").in;

    addr_t vr_obj = kutil::expr_addr_to_obj_addr(vr_obj_expr);
    assert(vb_obj == vr_obj);
    assert(kutil::solver_toolbox.are_exprs_always_equal(vb_index, vr_index));

    return kutil::solver_toolbox.are_exprs_always_equal(vb_value, vr_value);
  }

  bool table_data_from_vector_op(const bdd::Call *call_node, addr_t &obj,
                                 std::vector<klee::ref<klee::Expr>> &keys,
                                 klee::ref<klee::Expr> &value,
                                 std::optional<symbol_t> &hit) const {
    if (!is_vector_read(call_node)) {
      return false;
    }

    const call_t &call = call_node->get_call();
    assert(call.function_name == "vector_borrow");

    klee::ref<klee::Expr> vector_addr_expr = call.args.at("vector").expr;
    klee::ref<klee::Expr> index = call.args.at("index").in;
    klee::ref<klee::Expr> cell = call.extra_vars.at("borrowed_cell").second;

    obj = kutil::expr_addr_to_obj_addr(vector_addr_expr);
    keys.push_back(index);
    value = cell;

    return true;
  }

  bool table_data_from_dchain_op(const bdd::Call *call_node, addr_t &obj,
                                 std::vector<klee::ref<klee::Expr>> &keys,
                                 klee::ref<klee::Expr> &value,
                                 std::optional<symbol_t> &hit) const {
    const call_t &call = call_node->get_call();
    assert(call.function_name == "dchain_is_index_allocated" ||
           call.function_name == "dchain_rejuvenate_index");

    klee::ref<klee::Expr> dchain_addr_expr = call.args.at("chain").expr;
    klee::ref<klee::Expr> index = call.args.at("index").in;

    addr_t dchain_addr = kutil::expr_addr_to_obj_addr(dchain_addr_expr);

    obj = dchain_addr;
    keys.push_back(index);

    if (call.function_name == "dchain_is_index_allocated") {
      symbols_t symbols = call_node->get_locally_generated_symbols();
      symbol_t is_allocated;
      bool found =
          get_symbol(symbols, "dchain_is_index_allocated", is_allocated);
      assert(found && "Symbol dchain_is_index_allocated not found");

      hit = is_allocated;
    }

    return true;
  }
};

} // namespace tofino
} // namespace synapse
