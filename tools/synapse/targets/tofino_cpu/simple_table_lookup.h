#pragma once

#include "tofino_cpu_module.h"

namespace synapse {
namespace tofino_cpu {

using namespace synapse::tofino;

class SimpleTableLookup : public TofinoCPUModule {
private:
  addr_t obj;
  std::vector<klee::ref<klee::Expr>> keys;
  std::vector<klee::ref<klee::Expr>> values;
  std::optional<symbol_t> found;

public:
  SimpleTableLookup(const bdd::Node *node, addr_t _obj,
                    const std::vector<klee::ref<klee::Expr>> &_keys,
                    const std::vector<klee::ref<klee::Expr>> &_values,
                    const std::optional<symbol_t> &_found)
      : TofinoCPUModule(ModuleType::TofinoCPU_SimpleTableLookup,
                        "SimpleTableLookup", node),
        obj(_obj), keys(_keys), values(_values), found(_found) {}

  virtual void visit(EPVisitor &visitor, const EP *ep,
                     const EPNode *ep_node) const override {
    visitor.visit(ep, ep_node, this);
  }

  virtual Module *clone() const override {
    Module *cloned = new SimpleTableLookup(node, obj, keys, values, found);
    return cloned;
  }

  addr_t get_obj() const { return obj; }
  const std::vector<klee::ref<klee::Expr>> &get_keys() const { return keys; }
  const std::vector<klee::ref<klee::Expr>> &get_values() const {
    return values;
  }
  const std::optional<symbol_t> &get_found() const { return found; }
};

class SimpleTableLookupGenerator : public TofinoCPUModuleGenerator {
public:
  SimpleTableLookupGenerator()
      : TofinoCPUModuleGenerator(ModuleType::TofinoCPU_SimpleTableLookup,
                                 "SimpleTableLookup") {}

protected:
  virtual std::vector<const EP *>
  process_node(const EP *ep, const bdd::Node *node) const override {
    std::vector<const EP *> new_eps;

    if (node->get_type() != bdd::NodeType::CALL) {
      return new_eps;
    }

    const bdd::Call *call_node = static_cast<const bdd::Call *>(node);

    if (!is_simple_table(ep, call_node)) {
      return new_eps;
    }

    addr_t obj;
    std::vector<klee::ref<klee::Expr>> keys;
    std::vector<klee::ref<klee::Expr>> values;
    std::optional<symbol_t> found;
    get_table_lookup_data(call_node, obj, keys, values, found);

    Module *module = new SimpleTableLookup(node, obj, keys, values, found);
    EPNode *ep_node = new EPNode(module);

    EP *new_ep = new EP(*ep);
    new_eps.push_back(new_ep);

    EPLeaf leaf(ep_node, node->get_next());
    new_ep->process_leaf(ep_node, {leaf});

    return new_eps;
  }

private:
  bool is_simple_table(const EP *ep, const bdd::Call *call_node) const {
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

    return check_placement(ep, call_node, obj_arg,
                           PlacementDecision::TofinoSimpleTable);
  }

  void get_table_lookup_data(const bdd::Call *call_node, addr_t &obj,
                             std::vector<klee::ref<klee::Expr>> &keys,
                             std::vector<klee::ref<klee::Expr>> &values,
                             std::optional<symbol_t> &hit) const {
    const call_t &call = call_node->get_call();

    if (call.function_name == "map_get") {
      table_data_from_map_op(call_node, obj, keys, values, hit);
    } else if (call.function_name == "vector_borrow") {
      table_data_from_vector_op(call_node, obj, keys, values, hit);
    } else if (call.function_name == "dchain_is_index_allocated" ||
               call.function_name == "dchain_rejuvenate_index") {
      table_data_from_dchain_op(call_node, obj, keys, values, hit);
    } else {
      assert(false && "Unsupported function");
    }
  }

  void table_data_from_map_op(const bdd::Call *call_node, addr_t &obj,
                              std::vector<klee::ref<klee::Expr>> &keys,
                              std::vector<klee::ref<klee::Expr>> &values,
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
    keys = Table::build_keys(key);
    values = {value_out};
    hit = map_has_this_key;
  }

  void table_data_from_vector_op(const bdd::Call *call_node, addr_t &obj,
                                 std::vector<klee::ref<klee::Expr>> &keys,
                                 std::vector<klee::ref<klee::Expr>> &values,
                                 std::optional<symbol_t> &hit) const {
    // We can implement even if we later update the vector's contents!

    const call_t &call = call_node->get_call();
    assert(call.function_name == "vector_borrow");

    klee::ref<klee::Expr> vector_addr_expr = call.args.at("vector").expr;
    klee::ref<klee::Expr> index = call.args.at("index").expr;
    klee::ref<klee::Expr> cell = call.extra_vars.at("borrowed_cell").second;

    obj = kutil::expr_addr_to_obj_addr(vector_addr_expr);
    keys = {index};
    values = {cell};
  }

  void table_data_from_dchain_op(const bdd::Call *call_node, addr_t &obj,
                                 std::vector<klee::ref<klee::Expr>> &keys,
                                 std::vector<klee::ref<klee::Expr>> &values,
                                 std::optional<symbol_t> &hit) const {
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
  }
};

} // namespace tofino_cpu
} // namespace synapse
