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
      assert(data_structure->type == tofino::DSType::SIMPLE_TABLE);
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
    size_t num_entries;
    std::vector<klee::ref<klee::Expr>> keys;
    get_table_delete_data(ep, call_node, obj, num_entries, keys);

    Table table_mock = Table(0, num_entries, keys, {}, std::nullopt);
    if (!can_place_table(ep, &table_mock)) {
      return new_eps;
    }

    Module *module = new SimpleTableDelete(node, obj, keys);
    EPNode *ep_node = new EPNode(module);

    EP *new_ep = new EP(*ep);
    new_eps.push_back(new_ep);

    EPLeaf leaf(ep_node, node->get_next());
    new_ep->process_leaf(ep_node, {leaf});

    // Optimistically place the table as a Tofino SimpleTable, even if we are on
    // the CPU.
    // This allows other portions of the BDD (yet to be explored) to used the
    // Tofino SimpleTable implementation.
    // However, don't save the table just yet. That job belongs to the Tofino
    // modules.
    place(new_ep, obj, PlacementDecision::TofinoSimpleTable);

    return new_eps;
  }

private:
  bool can_place_table(const EP *ep, const Table *table) const {
    const TofinoContext *tofino_ctx = get_tofino_ctx(ep);
    std::unordered_set<DS_ID> dependencies =
        tofino_ctx->get_table_dependencies(ep);
    return tofino_ctx->check_table_placement(ep, table, dependencies);
  }

  bool can_place_in_simple_table(const EP *ep,
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
                     PlacementDecision::TofinoSimpleTable);
  }

  void get_table_delete_data(const EP *ep, const bdd::Call *call_node,
                             addr_t &obj, size_t &num_entries,
                             std::vector<klee::ref<klee::Expr>> &keys) const {
    const call_t &call = call_node->get_call();

    if (call.function_name == "map_erase") {
      table_delete_data_from_map_op(ep, call_node, obj, num_entries, keys);
    } else if (call.function_name == "dchain_free_index") {
      table_delete_data_from_dchain_op(ep, call_node, obj, num_entries, keys);
    } else {
      assert(false && "Unknown call");
    }
  }

  void table_delete_data_from_map_op(
      const EP *ep, const bdd::Call *call_node, addr_t &obj,
      size_t &num_entries, std::vector<klee::ref<klee::Expr>> &keys) const {
    const call_t &call = call_node->get_call();
    assert(call.function_name == "map_erase");

    klee::ref<klee::Expr> map_addr_expr = call.args.at("map").expr;
    klee::ref<klee::Expr> key = call.args.at("key").in;

    obj = kutil::expr_addr_to_obj_addr(map_addr_expr);
    keys = Table::build_keys(key);

    const Context &ctx = ep->get_ctx();
    const bdd::map_config_t &cfg = ctx.get_map_config(obj);
    num_entries = cfg.capacity;
  }

  void table_delete_data_from_dchain_op(
      const EP *ep, const bdd::Call *call_node, addr_t &obj,
      size_t &num_entries, std::vector<klee::ref<klee::Expr>> &keys) const {
    const call_t &call = call_node->get_call();
    assert(call.function_name == "dchain_free_index");

    klee::ref<klee::Expr> dchain_addr_expr = call.args.at("chain").expr;
    klee::ref<klee::Expr> index = call.args.at("index").expr;

    addr_t dchain_addr = kutil::expr_addr_to_obj_addr(dchain_addr_expr);

    obj = dchain_addr;
    keys.push_back(index);

    const Context &ctx = ep->get_ctx();
    const bdd::dchain_config_t &cfg = ctx.get_dchain_config(obj);
    num_entries = cfg.index_range;
  }
};

} // namespace tofino_cpu
} // namespace synapse
