#pragma once

#include "tofino_cpu_module.h"

namespace synapse {
namespace tofino_cpu {

using namespace synapse::tofino;

class CachedTableWrite : public TofinoCPUModule {
private:
  DS_ID id;
  addr_t obj;
  std::vector<klee::ref<klee::Expr>> keys;
  klee::ref<klee::Expr> value;

public:
  CachedTableWrite(const bdd::Node *node, DS_ID _id, addr_t _obj,
                   const std::vector<klee::ref<klee::Expr>> &_keys,
                   klee::ref<klee::Expr> _value)
      : TofinoCPUModule(ModuleType::TofinoCPU_CachedTableWrite,
                        "CachedTableWrite", node),
        id(_id), obj(_obj), keys(_keys), value(_value) {}

  virtual void visit(EPVisitor &visitor, const EP *ep,
                     const EPNode *ep_node) const override {
    visitor.visit(ep, ep_node, this);
  }

  virtual Module *clone() const {
    CachedTableWrite *cloned = new CachedTableWrite(node, id, obj, keys, value);
    return cloned;
  }

  DS_ID get_id() const { return id; }
  addr_t get_obj() const { return obj; }
  const std::vector<klee::ref<klee::Expr>> &get_keys() const { return keys; }
  klee::ref<klee::Expr> get_value() const { return value; }
};

class CachedTableWriteGenerator : public TofinoCPUModuleGenerator {
public:
  CachedTableWriteGenerator()
      : TofinoCPUModuleGenerator(ModuleType::TofinoCPU_CachedTableWrite,
                                 "CachedTableWrite") {}

protected:
  virtual std::optional<speculation_t>
  speculate(const EP *ep, const bdd::Node *node,
            const constraints_t &current_speculative_constraints,
            const Context &current_speculative_ctx) const override {
    if (node->get_type() != bdd::NodeType::CALL) {
      return std::nullopt;
    }

    const bdd::Call *call_node = static_cast<const bdd::Call *>(node);
    const call_t &call = call_node->get_call();

    if (call.function_name != "map_put") {
      return std::nullopt;
    }

    if (!can_place(ep, call_node, "map",
                   PlacementDecision::Tofino_CachedTable)) {
      return std::nullopt;
    }

    return current_speculative_ctx;
  }

  virtual std::vector<generator_product_t>
  process_node(const EP *ep, const bdd::Node *node) const override {
    std::vector<generator_product_t> products;

    if (node->get_type() != bdd::NodeType::CALL) {
      return products;
    }

    const bdd::Call *call_node = static_cast<const bdd::Call *>(node);
    const call_t &call = call_node->get_call();

    if (call.function_name != "map_put") {
      return products;
    }

    if (!check_placement(ep, call_node, "map",
                         PlacementDecision::Tofino_CachedTable)) {
      return products;
    }

    addr_t obj;
    std::vector<klee::ref<klee::Expr>> keys;
    klee::ref<klee::Expr> value;
    get_data(call_node, obj, keys, value);

    DS_ID id = get_cached_table_id(ep, obj);

    Module *module = new CachedTableWrite(node, id, obj, keys, value);
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
                klee::ref<klee::Expr> &value) const {
    const call_t &call = call_node->get_call();
    assert(call.function_name == "map_put");

    klee::ref<klee::Expr> map_addr_expr = call.args.at("map").expr;
    klee::ref<klee::Expr> key = call.args.at("key").in;
    value = call.args.at("value").expr;

    obj = kutil::expr_addr_to_obj_addr(map_addr_expr);
    keys = Table::build_keys(key);
    value = value;
  }
};

} // namespace tofino_cpu
} // namespace synapse
