#pragma once

#include "tofino_module.h"

namespace synapse {
namespace tofino {

class VectorRegisterLookup : public TofinoModule {
private:
  std::unordered_set<DS_ID> rids;
  addr_t obj;

public:
  VectorRegisterLookup(const bdd::Node *node,
                       const std::unordered_set<DS_ID> &_rids, addr_t _obj)
      : TofinoModule(ModuleType::Tofino_VectorRegisterLookup,
                     "VectorRegisterLookup", node),
        rids(_rids), obj(_obj) {}

  virtual void visit(EPVisitor &visitor, const EP *ep,
                     const EPNode *ep_node) const override {
    visitor.visit(ep, ep_node, this);
  }

  virtual Module *clone() const override {
    Module *cloned = new VectorRegisterLookup(node, rids, obj);
    return cloned;
  }

  const std::unordered_set<DS_ID> &get_rids() const { return rids; }
  addr_t get_obj() const { return obj; }

  virtual std::unordered_set<DS_ID> get_generated_ds() const override {
    return rids;
  }
};

class VectorRegisterLookupGenerator : public TofinoModuleGenerator {
public:
  VectorRegisterLookupGenerator()
      : TofinoModuleGenerator(ModuleType::Tofino_VectorRegisterLookup,
                              "VectorRegisterLookup") {}

protected:
  virtual std::optional<speculation_t>
  speculate(const EP *ep, const bdd::Node *node,
            const Context &ctx) const override {
    if (node->get_type() != bdd::NodeType::CALL) {
      return std::nullopt;
    }

    const bdd::Call *call_node = static_cast<const bdd::Call *>(node);
    const call_t &call = call_node->get_call();

    if (call.function_name != "vector_borrow") {
      return std::nullopt;
    }

    if (!is_vector_read(call_node)) {
      return std::nullopt;
    }

    if (!can_place(ep, call_node, "vector",
                   PlacementDecision::Tofino_VectorRegister)) {
      return std::nullopt;
    }

    vector_register_data_t vector_register_data =
        get_vector_register_data(ep, call_node);

    bool can_place_ds =
        can_get_or_build_vector_registers(ep, call_node, vector_register_data);

    if (!can_place_ds) {
      return std::nullopt;
    }

    const bdd::Node *vector_return =
        get_future_vector_return(ep, node, vector_register_data.obj);

    Context new_ctx = ctx;
    speculation_t speculation(new_ctx);

    if (vector_return) {
      speculation.skip.insert(vector_return->get_id());
    }

    return speculation;
  }

  virtual std::vector<__generator_product_t>
  process_node(const EP *ep, const bdd::Node *node) const override {
    std::vector<__generator_product_t> products;

    if (node->get_type() != bdd::NodeType::CALL) {
      return products;
    }

    const bdd::Call *call_node = static_cast<const bdd::Call *>(node);
    const call_t &call = call_node->get_call();

    if (call.function_name != "vector_borrow") {
      return products;
    }

    if (!is_vector_read(call_node)) {
      return products;
    }

    if (!can_place(ep, call_node, "vector",
                   PlacementDecision::Tofino_VectorRegister)) {
      return products;
    }

    vector_register_data_t vector_register_data =
        get_vector_register_data(ep, call_node);

    std::unordered_set<DS_ID> rids;
    std::unordered_set<DS_ID> deps;
    bool already_placed = false;

    std::unordered_set<DS *> regs = get_or_build_vector_registers(
        ep, call_node, vector_register_data, already_placed, rids, deps);

    if (regs.empty()) {
      return products;
    }

    Module *module =
        new VectorRegisterLookup(node, rids, vector_register_data.obj);
    EPNode *ep_node = new EPNode(module);

    EP *new_ep = new EP(*ep);
    products.emplace_back(new_ep);

    const bdd::Node *new_next;
    bdd::BDD *bdd = delete_future_vector_return(
        new_ep, node, vector_register_data.obj, new_next);

    if (!already_placed) {
      place_vector_registers(new_ep, vector_register_data, regs, deps);
    }

    EPLeaf leaf(ep_node, new_next);
    new_ep->process_leaf(ep_node, {leaf});
    new_ep->replace_bdd(bdd);

    new_ep->inspect();

    return products;
  }

private:
  vector_register_data_t get_vector_register_data(const EP *ep,
                                                  const bdd::Call *node) const {

    const call_t &call = node->get_call();

    klee::ref<klee::Expr> obj_expr = call.args.at("vector").expr;
    klee::ref<klee::Expr> index = call.args.at("index").expr;
    klee::ref<klee::Expr> value = call.extra_vars.at("borrowed_cell").second;

    addr_t obj = kutil::expr_addr_to_obj_addr(obj_expr);

    const Context &ctx = ep->get_ctx();
    const bdd::vector_config_t &cfg = ctx.get_vector_config(obj);

    vector_register_data_t vector_register_data = {
        .obj = obj,
        .num_entries = static_cast<int>(cfg.capacity),
        .index = index,
        .value = value,
        .actions = {RegisterAction::READ, RegisterAction::SWAP},
    };

    return vector_register_data;
  }

  const bdd::Call *get_future_vector_return(const EP *ep, const bdd::Node *node,
                                            addr_t vector) const {
    std::vector<const bdd::Call *> ops =
        get_future_functions(node, {"vector_return"});

    for (const bdd::Call *op : ops) {
      const call_t &call = op->get_call();
      assert(call.function_name == "vector_return");

      klee::ref<klee::Expr> obj_expr = call.args.at("vector").expr;
      addr_t obj = kutil::expr_addr_to_obj_addr(obj_expr);

      if (obj != vector) {
        continue;
      }

      return op;
    }

    return nullptr;
  }

  bdd::BDD *delete_future_vector_return(EP *ep, const bdd::Node *node,
                                        addr_t vector,
                                        const bdd::Node *&new_next) const {
    const bdd::BDD *old_bdd = ep->get_bdd();
    bdd::BDD *new_bdd = new bdd::BDD(*old_bdd);

    const bdd::Node *next = node->get_next();

    if (next) {
      new_next = new_bdd->get_node_by_id(next->get_id());
    } else {
      new_next = nullptr;
    }

    std::vector<const bdd::Call *> ops =
        get_future_functions(node, {"vector_return"});

    for (const bdd::Call *op : ops) {
      const call_t &call = op->get_call();
      assert(call.function_name == "vector_return");

      klee::ref<klee::Expr> obj_expr = call.args.at("vector").expr;
      addr_t obj = kutil::expr_addr_to_obj_addr(obj_expr);

      if (obj != vector) {
        continue;
      }

      bool replace_next = (op == next);
      bdd::Node *replacement;
      delete_non_branch_node_from_bdd(ep, new_bdd, op->get_id(), replacement);

      if (replace_next) {
        new_next = replacement;
      }
    }

    return new_bdd;
  }
};

} // namespace tofino
} // namespace synapse
