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
    const call_t &call = call_node->get_call();

    if (call.function_name != "map_get") {
      return new_eps;
    }

    klee::ref<klee::Expr> map_addr_expr = call.args.at("map").expr;
    klee::ref<klee::Expr> key = call.args.at("key").in;
    klee::ref<klee::Expr> value_out = call.args.at("value_out").out;

    symbols_t symbols = call_node->get_locally_generated_symbols();

    symbol_t map_has_this_key;
    bool found = get_symbol(symbols, "map_has_this_key", map_has_this_key);
    assert(found && "Symbol map_has_this_key not found");

    addr_t map_addr = kutil::expr_addr_to_obj_addr(map_addr_expr);

    const Context &ctx = ep->get_ctx();
    if (!ctx.can_place(map_addr, PlacementDecision::TofinoTableSimple)) {
      return new_eps;
    }

    int table_id = static_cast<int>(node->get_id());
    std::vector<klee::ref<klee::Expr>> keys = build_keys(key);
    Table *table = new Table(table_id, 0, keys, {value_out}, map_has_this_key);

    Module *module = new SimpleTableLookup(node, table_id, map_addr);
    EPNode *ep_node = new EPNode(module);

    EP *new_ep = new EP(*ep);
    new_eps.push_back(new_ep);

    Context &new_ctx = new_ep->get_mutable_ctx();
    TofinoContext *tofino_ctx = get_mutable_tofino_ctx(new_ep);

    new_ctx.save_placement(map_addr, PlacementDecision::TofinoTableSimple);
    tofino_ctx->add_data_structure(map_addr, table);

    EPLeaf leaf(ep_node, node->get_next());
    new_ep->process_leaf(ep_node, {leaf});

    return new_eps;
  }

private:
  std::vector<klee::ref<klee::Expr>>
  build_keys(klee::ref<klee::Expr> key) const {
    std::vector<klee::ref<klee::Expr>> keys;

    std::vector<kutil::expr_group_t> groups = kutil::get_expr_groups(key);
    for (const kutil::expr_group_t &group : groups) {
      keys.push_back(group.expr);
    }

    return keys;
  }
};

} // namespace tofino
} // namespace synapse
