#pragma once

#include "tofino_cpu_module.h"

namespace synapse {
namespace tofino_cpu {

class SimpleTableUpdate : public TofinoCPUModule {
private:
  addr_t obj;
  std::vector<klee::ref<klee::Expr>> keys;
  klee::ref<klee::Expr> value;
  bool internal_dchain_value;

public:
  SimpleTableUpdate(const bdd::Node *node, addr_t _obj,
                    const std::vector<klee::ref<klee::Expr>> &_keys,
                    klee::ref<klee::Expr> _value, bool _internal_dchain_value)
      : TofinoCPUModule(ModuleType::TofinoCPU_SimpleTableUpdate,
                        "SimpleTableUpdate", node),
        obj(_obj), keys(_keys), value(_value),
        internal_dchain_value(_internal_dchain_value) {}

  virtual void visit(EPVisitor &visitor, const EP *ep,
                     const EPNode *ep_node) const override {
    visitor.visit(ep, ep_node, this);
  }

  virtual Module *clone() const {
    SimpleTableUpdate *cloned =
        new SimpleTableUpdate(node, obj, keys, value, internal_dchain_value);
    return cloned;
  }

  addr_t get_obj() const { return obj; }
  const std::vector<klee::ref<klee::Expr>> &get_keys() const { return keys; }
  klee::ref<klee::Expr> get_value() const { return value; }
  bool has_internal_dchain_value() const { return internal_dchain_value; }

  std::vector<const tofino::Table *> get_tables(const EP *ep) {
    const Context &ctx = ep->get_ctx();
    const tofino::TofinoContext *tofino_ctx =
        ctx.get_target_ctx<tofino::TofinoContext>();
    const std::vector<tofino::DataStructure *> &data_structures =
        tofino_ctx->get_data_structures(obj);

    std::vector<const tofino::Table *> tables;
    for (const tofino::DataStructure *data_structure : data_structures) {
      assert(data_structure->type == tofino::DSType::SIMPLE_TABLE);
      const tofino::Table *table =
          static_cast<const tofino::Table *>(data_structure);
      tables.push_back(table);
    }

    return tables;
  }
};

class SimpleTableUpdateGenerator : public TofinoCPUModuleGenerator {
public:
  SimpleTableUpdateGenerator()
      : TofinoCPUModuleGenerator(ModuleType::TofinoCPU_SimpleTableUpdate,
                                 "SimpleTableUpdate") {}

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
    bool internal_dchain_value;
    get_table_update_data(call_node, obj, keys, value, internal_dchain_value);

    Module *module =
        new SimpleTableUpdate(node, obj, keys, value, internal_dchain_value);
    EPNode *ep_node = new EPNode(module);

    EP *new_ep = new EP(*ep);
    new_eps.push_back(new_ep);

    EPLeaf leaf(ep_node, node->get_next());
    new_ep->process_leaf(ep_node, {leaf});

    place(new_ep, obj, PlacementDecision::TofinoSimpleTable);

    return new_eps;
  }

private:
  bool can_place_in_simple_table(const EP *ep,
                                 const bdd::Call *call_node) const {
    const call_t &call = call_node->get_call();

    std::string obj_arg;
    if (call.function_name == "map_put") {
      obj_arg = "map";
    } else if (call.function_name == "vector_return") {
      obj_arg = "vector";
    } else if (call.function_name == "dchain_allocate_new_index") {
      obj_arg = "chain";
    } else {
      return false;
    }

    return can_place(ep, call_node, obj_arg,
                     PlacementDecision::TofinoSimpleTable);
  }

  void get_table_update_data(const bdd::Call *call_node, addr_t &obj,
                             std::vector<klee::ref<klee::Expr>> &keys,
                             klee::ref<klee::Expr> &value,
                             bool &internal_dchain_value) const {
    const call_t &call = call_node->get_call();

    if (call.function_name == "map_put") {
      table_update_data_from_map_op(call_node, obj, keys, value);
      internal_dchain_value = false;
    } else if (call.function_name == "vector_return") {
      table_update_data_from_vector_op(call_node, obj, keys, value);
      internal_dchain_value = false;
    } else if (call.function_name == "dchain_allocate_new_index") {
      table_data_from_dchain_op(call_node, obj, keys, value);
      internal_dchain_value = true;
    } else {
      assert(false && "Unknown call");
    }
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

  void table_update_data_from_map_op(const bdd::Call *call_node, addr_t &obj,
                                     std::vector<klee::ref<klee::Expr>> &keys,
                                     klee::ref<klee::Expr> &value) const {
    const call_t &call = call_node->get_call();
    assert(call.function_name == "map_put");

    klee::ref<klee::Expr> map_addr_expr = call.args.at("map").expr;
    klee::ref<klee::Expr> key = call.args.at("key").in;
    value = call.args.at("value").out;

    obj = kutil::expr_addr_to_obj_addr(map_addr_expr);
    keys = build_keys(key);
  }

  void
  table_update_data_from_vector_op(const bdd::Call *call_node, addr_t &obj,
                                   std::vector<klee::ref<klee::Expr>> &keys,
                                   klee::ref<klee::Expr> &value) const {
    const call_t &call = call_node->get_call();
    assert(call.function_name == "vector_return");

    klee::ref<klee::Expr> vector_addr_expr = call.args.at("vector").expr;
    klee::ref<klee::Expr> index = call.args.at("index").in;
    value = call.args.at("value").in;

    obj = kutil::expr_addr_to_obj_addr(vector_addr_expr);
    keys.push_back(index);
  }

  void table_data_from_dchain_op(const bdd::Call *call_node, addr_t &obj,
                                 std::vector<klee::ref<klee::Expr>> &keys,
                                 klee::ref<klee::Expr> &value) const {
    const call_t &call = call_node->get_call();
    assert(call.function_name == "dchain_allocate_new_index");

    klee::ref<klee::Expr> dchain_addr_expr = call.args.at("chain").expr;
    klee::ref<klee::Expr> index_out = call.args.at("index_out").out;

    addr_t dchain_addr = kutil::expr_addr_to_obj_addr(dchain_addr_expr);

    obj = dchain_addr;
    keys.push_back(index_out);
    // No value, the index is actually the table key
  }
};

} // namespace tofino_cpu
} // namespace synapse
