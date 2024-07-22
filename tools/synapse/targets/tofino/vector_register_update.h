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
  virtual std::optional<speculation_t>
  speculate(const EP *ep, const bdd::Node *node,
            const Context &ctx) const override {
    if (node->get_type() != bdd::NodeType::CALL) {
      return std::nullopt;
    }

    const bdd::Call *vector_borrow = static_cast<const bdd::Call *>(node);
    const call_t &call = vector_borrow->get_call();

    if (call.function_name != "vector_borrow") {
      return std::nullopt;
    }

    if (is_vector_read(vector_borrow)) {
      return std::nullopt;
    }

    const bdd::Call *vector_return;
    if (is_conditional_write(vector_borrow, vector_return)) {
      return std::nullopt;
    }

    if (!can_place(ep, vector_borrow, "vector",
                   PlacementDecision::Tofino_VectorRegister)) {
      return std::nullopt;
    }

    std::vector<modification_t> changes;
    vector_register_data_t vector_register_data =
        get_vector_register_data(ep, vector_borrow, vector_return, changes);

    // This will be ignored by the Ignore module.
    if (changes.empty()) {
      return std::nullopt;
    }

    bool can_place_ds = can_get_or_build_vector_registers(ep, vector_borrow,
                                                          vector_register_data);

    if (!can_place_ds) {
      return std::nullopt;
    }

    Context new_ctx = ctx;
    speculation_t speculation(new_ctx);
    speculation.skip.insert(vector_return->get_id());

    return speculation;
  }

  virtual std::vector<__generator_product_t>
  process_node(const EP *ep, const bdd::Node *node) const override {
    std::vector<__generator_product_t> products;

    if (node->get_type() != bdd::NodeType::CALL) {
      return products;
    }

    const bdd::Call *vector_borrow = static_cast<const bdd::Call *>(node);
    const call_t &call = vector_borrow->get_call();

    if (call.function_name != "vector_borrow") {
      return products;
    }

    if (is_vector_read(vector_borrow)) {
      return products;
    }

    const bdd::Call *vector_return;
    if (is_conditional_write(vector_borrow, vector_return)) {
      return products;
    }

    if (!can_place(ep, vector_borrow, "vector",
                   PlacementDecision::Tofino_VectorRegister)) {
      return products;
    }

    std::vector<modification_t> changes;
    vector_register_data_t vector_register_data =
        get_vector_register_data(ep, vector_borrow, vector_return, changes);

    // Check the Ignore module.
    if (changes.empty()) {
      return products;
    }

    std::unordered_set<DS_ID> rids;
    std::unordered_set<DS_ID> deps;
    bool already_placed = false;

    std::unordered_set<DS *> regs = get_or_build_vector_registers(
        ep, vector_borrow, vector_register_data, already_placed, rids, deps);

    if (regs.empty()) {
      return products;
    }

    Module *module = new VectorRegisterUpdate(
        node, rids, vector_register_data.obj, vector_register_data.index,
        vector_register_data.value, changes);
    EPNode *ep_node = new EPNode(module);

    EP *new_ep = new EP(*ep);
    products.emplace_back(new_ep);

    const bdd::Node *new_next;
    bdd::BDD *bdd =
        delete_future_vector_return(new_ep, node, vector_return, new_next);

    if (!already_placed) {
      place_vector_registers(new_ep, vector_register_data, regs, deps);
    }

    EPLeaf leaf(ep_node, new_next);
    new_ep->process_leaf(ep_node, {leaf});
    new_ep->replace_bdd(bdd);

    // new_ep->inspect_debug();

    return products;
  }

private:
  bool is_conditional_write(const bdd::Call *node,
                            const bdd::Call *&vector_return) const {
    std::vector<const bdd::Call *> vector_returns =
        get_future_vector_return(node);
    if (vector_returns.size() != 1) {
      return true;
    }

    vector_return = vector_returns.at(0);
    return false;
  }

  vector_register_data_t
  get_vector_register_data(const EP *ep, const bdd::Call *vector_borrow,
                           const bdd::Call *vector_return,
                           std::vector<modification_t> &changes) const {
    const call_t &call = vector_borrow->get_call();

    klee::ref<klee::Expr> obj_expr = call.args.at("vector").expr;
    klee::ref<klee::Expr> index = call.args.at("index").expr;
    klee::ref<klee::Expr> value = call.extra_vars.at("borrowed_cell").second;

    addr_t obj = kutil::expr_addr_to_obj_addr(obj_expr);
    changes = build_vector_modifications(vector_borrow, vector_return);

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

  bdd::BDD *delete_future_vector_return(EP *ep, const bdd::Node *node,
                                        const bdd::Node *vector_return,
                                        const bdd::Node *&new_next) const {
    const bdd::BDD *old_bdd = ep->get_bdd();
    bdd::BDD *new_bdd = new bdd::BDD(*old_bdd);

    const bdd::Node *next = node->get_next();

    if (next) {
      new_next = new_bdd->get_node_by_id(next->get_id());
    } else {
      new_next = nullptr;
    }

    bool replace_next = (vector_return == next);
    bdd::Node *replacement;
    delete_non_branch_node_from_bdd(ep, new_bdd, vector_return->get_id(),
                                    replacement);

    if (replace_next) {
      new_next = replacement;
    }

    return new_bdd;
  }
};

} // namespace tofino
} // namespace synapse
