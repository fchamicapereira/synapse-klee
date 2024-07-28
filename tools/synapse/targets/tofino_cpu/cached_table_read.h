#pragma once

#include "tofino_cpu_module.h"

namespace synapse {
namespace tofino_cpu {

using namespace synapse::tofino;

class TTLCachedTableRead : public TofinoCPUModule {
private:
  DS_ID id;
  addr_t obj;
  std::vector<klee::ref<klee::Expr>> keys;
  klee::ref<klee::Expr> value;
  std::optional<symbol_t> found;

public:
  TTLCachedTableRead(const bdd::Node *node, DS_ID _id, addr_t _obj,
                     const std::vector<klee::ref<klee::Expr>> &_keys,
                     klee::ref<klee::Expr> _value,
                     const std::optional<symbol_t> &_found)
      : TofinoCPUModule(ModuleType::TofinoCPU_TTLCachedTableRead,
                        "TTLCachedTableRead", node),
        id(_id), obj(_obj), keys(_keys), value(_value), found(_found) {}

  virtual void visit(EPVisitor &visitor, const EP *ep,
                     const EPNode *ep_node) const override {
    visitor.visit(ep, ep_node, this);
  }

  virtual Module *clone() const override {
    Module *cloned = new TTLCachedTableRead(node, id, obj, keys, value, found);
    return cloned;
  }

  DS_ID get_id() const { return id; }
  addr_t get_obj() const { return obj; }
  const std::vector<klee::ref<klee::Expr>> &get_keys() const { return keys; }
  klee::ref<klee::Expr> get_value() const { return value; }
  const std::optional<symbol_t> &get_found() const { return found; }
};

class TTLCachedTableReadGenerator : public TofinoCPUModuleGenerator {
public:
  TTLCachedTableReadGenerator()
      : TofinoCPUModuleGenerator(ModuleType::TofinoCPU_TTLCachedTableRead,
                                 "TTLCachedTableRead") {}

protected:
  virtual std::optional<speculation_t>
  speculate(const EP *ep, const bdd::Node *node,
            const Context &ctx) const override {
    if (node->get_type() != bdd::NodeType::CALL) {
      return std::nullopt;
    }

    const bdd::Call *call_node = static_cast<const bdd::Call *>(node);
    const call_t &call = call_node->get_call();

    if (call.function_name != "map_get") {
      return std::nullopt;
    }

    if (!can_place(ep, call_node, "map",
                   PlacementDecision::Tofino_TTLCachedTable)) {
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
    const call_t &call = call_node->get_call();

    if (call.function_name != "map_get") {
      return products;
    }

    if (!check_placement(ep, call_node, "map",
                         PlacementDecision::Tofino_TTLCachedTable)) {
      return products;
    }

    addr_t obj;
    std::vector<klee::ref<klee::Expr>> keys;
    klee::ref<klee::Expr> value;
    std::optional<symbol_t> found;
    get_data(call_node, obj, keys, value, found);

    DS_ID id = get_cached_table_id(ep, obj);

    Module *module = new TTLCachedTableRead(node, id, obj, keys, value, found);
    EPNode *ep_node = new EPNode(module);

    EP *new_ep = new EP(*ep);
    products.emplace_back(new_ep);

    EPLeaf leaf(ep_node, node->get_next());
    new_ep->process_leaf(ep_node, {leaf});

    return products;
  }

private:
  DS_ID get_cached_table_id(const EP *ep, addr_t obj) const {
    const Context &ctx = ep->get_ctx();
    const tofino::TofinoContext *tofino_ctx =
        ctx.get_target_ctx<tofino::TofinoContext>();
    const std::vector<tofino::DS *> &data_structures = tofino_ctx->get_ds(obj);
    assert(data_structures.size() == 1);
    assert(data_structures[0]->type == tofino::DSType::CACHED_TABLE);
    return data_structures[0]->id;
  }

  void get_data(const bdd::Call *call_node, addr_t &obj,
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
    keys = Table::build_keys(key);
    value = value_out;
    hit = map_has_this_key;
  }
};

} // namespace tofino_cpu
} // namespace synapse
