#pragma once

#include "tofino_cpu_module.h"

namespace synapse {
namespace tofino_cpu {

using namespace synapse::tofino;

class SimpleTableDelete : public TofinoCPUModule {
private:
  addr_t obj;
  std::vector<klee::ref<klee::Expr>> keys;

public:
  SimpleTableDelete(const bdd::Node *node, addr_t _obj,
                    const std::vector<klee::ref<klee::Expr>> &_keys)
      : TofinoCPUModule(ModuleType::TofinoCPU_SimpleTableDelete,
                        "SimpleTableDelete", node),
        obj(_obj), keys(_keys) {}

  virtual void visit(EPVisitor &visitor, const EP *ep,
                     const EPNode *ep_node) const override {
    visitor.visit(ep, ep_node, this);
  }

  virtual Module *clone() const {
    SimpleTableDelete *cloned = new SimpleTableDelete(node, obj, keys);
    return cloned;
  }

  addr_t get_obj() const { return obj; }
  const std::vector<klee::ref<klee::Expr>> &get_keys() const { return keys; }

  std::vector<const tofino::Table *> get_tables(const EP *ep) {
    const Context &ctx = ep->get_ctx();
    const tofino::TofinoContext *tofino_ctx =
        ctx.get_target_ctx<tofino::TofinoContext>();
    const std::vector<tofino::DS *> &data_structures = tofino_ctx->get_ds(obj);

    std::vector<const tofino::Table *> tables;
    for (const tofino::DS *data_structure : data_structures) {
      assert(data_structure->type == tofino::DSType::TABLE);
      const tofino::Table *table =
          static_cast<const tofino::Table *>(data_structure);
      tables.push_back(table);
    }

    return tables;
  }
};

class SimpleTableDeleteGenerator : public TofinoCPUModuleGenerator {
public:
  SimpleTableDeleteGenerator()
      : TofinoCPUModuleGenerator(ModuleType::TofinoCPU_SimpleTableDelete,
                                 "SimpleTableDelete") {}

protected:
  virtual std::optional<speculation_t>
  speculate(const EP *ep, const bdd::Node *node,
            const Context &ctx) const override {
    if (node->get_type() != bdd::NodeType::CALL) {
      return std::nullopt;
    }

    const bdd::Call *call_node = static_cast<const bdd::Call *>(node);

    if (!check_simple_table_delete_placement(ep, call_node)) {
      return std::nullopt;
    }

    return ctx;
  }

  virtual std::vector<__generator_product_t>
  process_node(const EP *ep, const bdd::Node *node) const override {
    std::vector<__generator_product_t> products;

    if (node->get_type() != bdd::NodeType::CALL) {
      return products;
    }

    const bdd::Call *call_node = static_cast<const bdd::Call *>(node);

    if (!can_place_simple_table_delete(ep, call_node)) {
      return products;
    }

    addr_t obj;
    std::vector<klee::ref<klee::Expr>> keys;
    get_table_delete_data(call_node, obj, keys);

    Module *module = new SimpleTableDelete(node, obj, keys);
    EPNode *ep_node = new EPNode(module);

    EP *new_ep = new EP(*ep);
    products.emplace_back(new_ep);

    EPLeaf leaf(ep_node, node->get_next());
    new_ep->process_leaf(ep_node, {leaf});

    return products;
  }

private:
  bool check_simple_table_delete_placement(const EP *ep,
                                           const bdd::Call *call_node) const {
    const call_t &call = call_node->get_call();

    std::string obj_arg;
    if (call.function_name == "map_erase") {
      obj_arg = "map";
    } else if (call.function_name == "dchain_free_index") {
      obj_arg = "chain";
    } else {
      return false;
    }

    return check_placement(ep, call_node, obj_arg,
                           PlacementDecision::Tofino_SimpleTable);
  }

  bool can_place_simple_table_delete(const EP *ep,
                                     const bdd::Call *call_node) const {
    const call_t &call = call_node->get_call();

    std::string obj_arg;
    if (call.function_name == "map_erase") {
      obj_arg = "map";
    } else if (call.function_name == "dchain_free_index") {
      obj_arg = "chain";
    } else {
      return false;
    }

    return can_place(ep, call_node, obj_arg,
                     PlacementDecision::Tofino_SimpleTable);
  }

  void get_table_delete_data(const bdd::Call *call_node, addr_t &obj,
                             std::vector<klee::ref<klee::Expr>> &keys) const {
    const call_t &call = call_node->get_call();

    if (call.function_name == "map_erase") {
      table_delete_data_from_map_op(call_node, obj, keys);
    } else if (call.function_name == "dchain_free_index") {
      table_delete_data_from_dchain_op(call_node, obj, keys);
    } else {
      assert(false && "Unknown call");
    }
  }

  void table_delete_data_from_map_op(
      const bdd::Call *call_node, addr_t &obj,
      std::vector<klee::ref<klee::Expr>> &keys) const {
    const call_t &call = call_node->get_call();
    assert(call.function_name == "map_erase");

    klee::ref<klee::Expr> map_addr_expr = call.args.at("map").expr;
    klee::ref<klee::Expr> key = call.args.at("key").in;

    obj = kutil::expr_addr_to_obj_addr(map_addr_expr);
    keys = Table::build_keys(key);
  }

  void table_delete_data_from_dchain_op(
      const bdd::Call *call_node, addr_t &obj,
      std::vector<klee::ref<klee::Expr>> &keys) const {
    const call_t &call = call_node->get_call();
    assert(call.function_name == "dchain_free_index");

    klee::ref<klee::Expr> dchain_addr_expr = call.args.at("chain").expr;
    klee::ref<klee::Expr> index = call.args.at("index").expr;

    addr_t dchain_addr = kutil::expr_addr_to_obj_addr(dchain_addr_expr);

    obj = dchain_addr;
    keys.push_back(index);
  }
};

} // namespace tofino_cpu
} // namespace synapse
