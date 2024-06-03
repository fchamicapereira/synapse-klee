#pragma once

#include "tofino_cpu_module.h"

namespace synapse {
namespace tofino_cpu {

class SimpleTableLookup : public TofinoCPUModule {
private:
  addr_t obj;
  std::vector<klee::ref<klee::Expr>> keys;
  klee::ref<klee::Expr> value;
  std::optional<symbol_t> found;

public:
  SimpleTableLookup(const bdd::Node *node, addr_t _obj,
                    const std::vector<klee::ref<klee::Expr>> &_keys,
                    klee::ref<klee::Expr> _value,
                    const std::optional<symbol_t> &_found)
      : TofinoCPUModule(ModuleType::TofinoCPU_SimpleTableLookup,
                        "SimpleTableLookup", node),
        obj(_obj), keys(_keys), value(_value), found(_found) {}

  virtual void visit(EPVisitor &visitor, const EP *ep,
                     const EPNode *ep_node) const override {
    visitor.visit(ep, ep_node, this);
  }

  virtual Module *clone() const override {
    Module *cloned = new SimpleTableLookup(node, obj, keys, value, found);
    return cloned;
  }

  addr_t get_obj() const { return obj; }
  const std::vector<klee::ref<klee::Expr>> &get_keys() const { return keys; }
  klee::ref<klee::Expr> get_value() const { return value; }
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

    if (!can_place_in_simple_table(ep, call_node)) {
      return new_eps;
    }

    addr_t obj;
    std::vector<klee::ref<klee::Expr>> keys;
    klee::ref<klee::Expr> value;
    std::optional<symbol_t> found;

    if (!table_data_from_call(call_node, obj, keys, value, found)) {
      return new_eps;
    }

    Module *module = new SimpleTableLookup(node, obj, keys, value, found);
    EPNode *ep_node = new EPNode(module);

    EP *new_ep = new EP(*ep);
    new_eps.push_back(new_ep);

    EPLeaf leaf(ep_node, node->get_next());
    new_ep->process_leaf(ep_node, {leaf});

    // Optimistically place the table as a Tofino SimpleTable, even if we are on
    // the CPU.
    // This allows other portions of the BDD (yet to be explored) to used the
    // Tofino SimpleTable implementation.
    place(new_ep, obj, PlacementDecision::TofinoSimpleTable);

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
    } else {
      return false;
    }

    return can_place(ep, call_node, obj_arg,
                     PlacementDecision::TofinoSimpleTable);
  }

  bool table_data_from_call(const bdd::Call *call_node, addr_t &obj,
                            std::vector<klee::ref<klee::Expr>> &keys,
                            klee::ref<klee::Expr> &value,
                            std::optional<symbol_t> &hit) const {
    const call_t &call = call_node->get_call();

    if (call.function_name == "map_get") {
      return table_data_from_map_get(call_node, obj, keys, value, hit);
    }

    if (call.function_name == "vector_borrow") {
      return table_data_from_vector_borrow(call_node, obj, keys, value, hit);
    }

    return false;
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

  bool table_data_from_map_get(const bdd::Call *call_node, addr_t &obj,
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

  bool table_data_from_vector_borrow(const bdd::Call *call_node, addr_t &obj,
                                     std::vector<klee::ref<klee::Expr>> &keys,
                                     klee::ref<klee::Expr> &value,
                                     std::optional<symbol_t> &hit) const {
    // We can place even if we later update the vector's contents!

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
};

} // namespace tofino_cpu
} // namespace synapse
