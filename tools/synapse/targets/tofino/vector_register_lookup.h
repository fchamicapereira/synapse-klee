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
                   PlacementDecision::Tofino_VectorRegister)) {
      return new_eps;
    }

    addr_t obj;
    klee::ref<klee::Expr> index;
    klee::ref<klee::Expr> value;
    int num_entries;
    int index_size;

    get_data(ep, call_node, obj, index, value, num_entries, index_size);

    std::unordered_set<DS_ID> rids;
    std::unordered_set<DS *> regs;
    std::unordered_set<DS_ID> deps;

    bool regs_already_placed = check_placement(
        ep, call_node, "vector", PlacementDecision::Tofino_VectorRegister);

    if (regs_already_placed) {
      regs = get_registers(ep, node, obj, rids, deps);
    } else {
      std::unordered_set<RegisterAction> actions = {RegisterAction::READ,
                                                    RegisterAction::SWAP};
      regs = build_registers(ep, node, num_entries, index_size, value, actions,
                             rids, deps);
    }

    if (regs.empty()) {
      return new_eps;
    }

    Module *module = new VectorRegisterLookup(node, rids, obj);
    EPNode *ep_node = new EPNode(module);

    EP *new_ep = new EP(*ep);
    new_eps.push_back(new_ep);

    const bdd::Node *new_next;
    bdd::BDD *bdd = delete_future_vector_return(new_ep, node, obj, new_next);

    EPLeaf leaf(ep_node, new_next);
    new_ep->process_leaf(ep_node, {leaf});
    new_ep->replace_bdd(bdd);

    new_ep->inspect();

    if (!regs_already_placed) {
      place_regs(new_ep, obj, regs, deps);
    }

    return new_eps;
  }

private:
  void get_data(const EP *ep, const bdd::Call *node, addr_t &obj,
                klee::ref<klee::Expr> &index, klee::ref<klee::Expr> &value,
                int &num_entries, int &index_size) const {
    const call_t &call = node->get_call();
    klee::ref<klee::Expr> obj_expr = call.args.at("vector").expr;

    obj = kutil::expr_addr_to_obj_addr(obj_expr);
    index = call.args.at("index").expr;
    value = call.extra_vars.at("borrowed_cell").second;

    const Context &ctx = ep->get_ctx();
    const bdd::vector_config_t &cfg = ctx.get_vector_config(obj);

    num_entries = cfg.capacity;
    index_size = index->getWidth();
  }

  void place_regs(EP *ep, addr_t obj, const std::unordered_set<DS *> &regs,
                  const std::unordered_set<DS_ID> &deps) const {
    TofinoContext *tofino_ctx = get_mutable_tofino_ctx(ep);
    place(ep, obj, PlacementDecision::Tofino_VectorRegister);
    tofino_ctx->place_many(ep, obj, {regs}, deps);
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

    std::vector<const bdd::Node *> ops =
        get_future_functions(node, {"vector_return"});

    for (const bdd::Node *op : ops) {
      assert(op->get_type() == bdd::NodeType::CALL);

      const bdd::Call *call_node = static_cast<const bdd::Call *>(op);
      const call_t &call = call_node->get_call();
      assert(call.function_name == "vector_return");

      klee::ref<klee::Expr> obj_expr = call.args.at("vector").expr;
      addr_t obj = kutil::expr_addr_to_obj_addr(obj_expr);

      if (obj != vector) {
        continue;
      }

      bool replace_next = (op == next);
      bdd::Node *replacement;
      delete_non_branch_node_from_bdd(ep, new_bdd, op, replacement);

      if (replace_next) {
        new_next = replacement;
      }
    }

    return new_bdd;
  }
};

} // namespace tofino
} // namespace synapse
