#pragma once

#include "tofino_module.h"

namespace synapse {
namespace tofino {

class VectorRegisterUpdate : public TofinoModule {
private:
  std::unordered_set<DS_ID> rids;
  addr_t obj;
  klee::ref<klee::Expr> index;
  klee::ref<klee::Expr> read_value;
  std::vector<modification_t> modifications;

public:
  VectorRegisterUpdate(const bdd::Node *node,
                       const std::unordered_set<DS_ID> &_rids, addr_t _obj,
                       klee::ref<klee::Expr> _index,
                       klee::ref<klee::Expr> _read_value,
                       const std::vector<modification_t> &_modifications)
      : TofinoModule(ModuleType::Tofino_VectorRegisterUpdate,
                     "VectorRegisterUpdate", node),
        rids(_rids), obj(_obj), index(_index), read_value(_read_value),
        modifications(_modifications) {}

  virtual void visit(EPVisitor &visitor, const EP *ep,
                     const EPNode *ep_node) const override {
    visitor.visit(ep, ep_node, this);
  }

  virtual Module *clone() const override {
    Module *cloned = new VectorRegisterUpdate(node, rids, obj, index,
                                              read_value, modifications);
    return cloned;
  }

  const std::unordered_set<DS_ID> &get_rids() const { return rids; }
  addr_t get_obj() const { return obj; }
  klee::ref<klee::Expr> get_index() const { return index; }
  klee::ref<klee::Expr> get_read_value() const { return read_value; }
  const std::vector<modification_t> &get_modifications() const {
    return modifications;
  }

  virtual std::unordered_set<DS_ID> get_generated_ds() const override {
    return rids;
  }
};

class VectorRegisterUpdateGenerator : public TofinoModuleGenerator {
public:
  VectorRegisterUpdateGenerator()
      : TofinoModuleGenerator(ModuleType::Tofino_VectorRegisterUpdate,
                              "VectorRegisterUpdate") {}

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

    if (is_vector_read(call_node)) {
      return new_eps;
    }

    if (!can_place(ep, call_node, "vector",
                   PlacementDecision::Tofino_VectorRegister)) {
      return new_eps;
    }

    addr_t obj;
    klee::ref<klee::Expr> index;
    klee::ref<klee::Expr> value;
    const bdd::Node *vector_return;
    std::vector<modification_t> changes;
    int num_entries;
    int index_size;

    get_data(ep, call_node, obj, index, value, vector_return, changes,
             num_entries, index_size);

    // Check the Ignore module.
    if (changes.empty()) {
      return new_eps;
    }

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

    Module *module =
        new VectorRegisterUpdate(node, rids, obj, index, value, changes);
    EPNode *ep_node = new EPNode(module);

    EP *new_ep = new EP(*ep);
    new_eps.push_back(new_ep);

    EPLeaf leaf(ep_node, node->get_next());
    new_ep->process_leaf(ep_node, {leaf});

    // No need to process the future vector_return, we already did the
    // modifications.
    new_ep->process_future_non_branch_node(vector_return);

    if (!regs_already_placed) {
      place_regs(new_ep, obj, regs, deps);
    }

    return new_eps;
  }

private:
  void get_data(const EP *ep, const bdd::Call *node, addr_t &obj,
                klee::ref<klee::Expr> &index, klee::ref<klee::Expr> &value,
                const bdd::Node *&vector_return,
                std::vector<modification_t> &changes, int &num_entries,
                int &index_size) const {
    const call_t &call = node->get_call();
    klee::ref<klee::Expr> obj_expr = call.args.at("vector").expr;

    obj = kutil::expr_addr_to_obj_addr(obj_expr);
    index = call.args.at("index").expr;
    value = call.extra_vars.at("borrowed_cell").second;
    vector_return = get_future_vector_return(node, obj);
    changes = get_modifications(node, vector_return);

    assert(vector_return && "vector_return not found");

    const Context &ctx = ep->get_ctx();
    const bdd::vector_config_t &cfg = ctx.get_vector_config(obj);

    num_entries = cfg.capacity;
    index_size = index->getWidth();
  }

  std::vector<modification_t>
  get_modifications(const bdd::Node *vector_borrow,
                    const bdd::Node *vector_return) const {
    assert(vector_borrow->get_type() == bdd::NodeType::CALL);
    assert(vector_return->get_type() == bdd::NodeType::CALL);

    const bdd::Call *vb = static_cast<const bdd::Call *>(vector_borrow);
    const bdd::Call *vr = static_cast<const bdd::Call *>(vector_return);

    const call_t &vb_call = vb->get_call();
    const call_t &vr_call = vr->get_call();

    klee::ref<klee::Expr> original_value =
        vb_call.extra_vars.at("borrowed_cell").second;
    klee::ref<klee::Expr> value = vr_call.args.at("value").in;

    std::vector<modification_t> changes =
        build_modifications(original_value, value);

    return changes;
  }

  void place_regs(EP *ep, addr_t obj, const std::unordered_set<DS *> &regs,
                  const std::unordered_set<DS_ID> &deps) const {
    TofinoContext *tofino_ctx = get_mutable_tofino_ctx(ep);
    place(ep, obj, PlacementDecision::Tofino_VectorRegister);
    tofino_ctx->place_many(ep, obj, {regs}, deps);
  }
};

} // namespace tofino
} // namespace synapse
