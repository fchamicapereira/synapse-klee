#pragma once

#include "tofino_module.h"

namespace synapse {
namespace tofino {

class VectorRegisterLookup : public TofinoModule {
private:
  DS_ID register_id;
  addr_t obj;

public:
  VectorRegisterLookup(const bdd::Node *node, DS_ID _register_id, addr_t _obj)
      : TofinoModule(ModuleType::Tofino_VectorRegisterLookup,
                     "VectorRegisterLookup", node),
        register_id(_register_id), obj(_obj) {}

  virtual void visit(EPVisitor &visitor, const EP *ep,
                     const EPNode *ep_node) const override {
    visitor.visit(ep, ep_node, this);
  }

  virtual Module *clone() const override {
    Module *cloned = new VectorRegisterLookup(node, register_id, obj);
    return cloned;
  }

  int get_register_id() const { return register_id; }
  addr_t get_obj() const { return obj; }

  virtual std::unordered_set<DS_ID> get_generated_ds() const override {
    return {register_id};
  }
};

class VectorRegisterLookupGenerator : public TofinoModuleGenerator {
public:
  VectorRegisterLookupGenerator()
      : TofinoModuleGenerator(ModuleType::Tofino_VectorRegisterLookup,
                              "VectorRegisterLookup") {}

protected:
  virtual std::vector<const EP *>
  process_node(const EP *ep, const bdd::Node *node) const override {
    std::vector<const EP *> new_eps;

    if (node->get_type() != bdd::NodeType::CALL) {
      return new_eps;
    }

    const bdd::Call *call_node = static_cast<const bdd::Call *>(node);
    const call_t &call = call_node->get_call();

    if (call.function_name != "vector_borrow") {
      return new_eps;
    }

    if (!is_vector_read(call_node)) {
      return new_eps;
    }

    if (!can_place(ep, call_node, "vector",
                   PlacementDecision::TofinoRegister)) {
      return new_eps;
    }

    klee::ref<klee::Expr> vector_addr_expr = call.args.at("vector").expr;
    klee::ref<klee::Expr> index = call.args.at("index").expr;
    klee::ref<klee::Expr> value = call.extra_vars.at("borrowed_cell").second;

    addr_t vector_addr = kutil::expr_addr_to_obj_addr(vector_addr_expr);

    const Context &ctx = ep->get_ctx();
    const bdd::vector_config_t &cfg = ctx.get_vector_config(vector_addr);

    int num_entries = cfg.capacity;
    int index_size = index->getWidth();

    DS_ID register_id = static_cast<DS_ID>(node->get_id());
    Register *reg =
        new Register(register_id, num_entries, 1, index_size, value);

    const TofinoContext *tofino_ctx = get_tofino_ctx(ep);
    std::unordered_set<DS_ID> deps = tofino_ctx->get_stateful_deps(ep);

    if (!tofino_ctx->check_register_placement(ep, reg, deps)) {
      delete reg;
      return new_eps;
    }

    Module *module = new VectorRegisterLookup(node, register_id, vector_addr);
    EPNode *ep_node = new EPNode(module);

    EP *new_ep = new EP(*ep);
    new_eps.push_back(new_ep);

    EPLeaf leaf(ep_node, node->get_next());
    new_ep->process_leaf(ep_node, {leaf});

    place_register(new_ep, vector_addr, reg, deps);

    return new_eps;
  }

private:
  void place_register(EP *ep, addr_t obj, Register *reg,
                      const std::unordered_set<DS_ID> &deps) const {
    TofinoContext *tofino_ctx = get_mutable_tofino_ctx(ep);
    place(ep, obj, PlacementDecision::TofinoRegister);
    tofino_ctx->save_register(ep, obj, reg, deps);
    DEBUG_PAUSE
  }
};

} // namespace tofino
} // namespace synapse
