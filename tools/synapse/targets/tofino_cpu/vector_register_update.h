#pragma once

#include "tofino_cpu_module.h"

namespace synapse {
namespace tofino_cpu {

class VectorRegisterUpdate : public TofinoCPUModule {
private:
  addr_t obj;
  klee::ref<klee::Expr> index;
  addr_t value_addr;
  std::vector<modification_t> modifications;

public:
  VectorRegisterUpdate(const bdd::Node *node, addr_t _obj,
                       klee::ref<klee::Expr> _index, addr_t _value_addr,
                       const std::vector<modification_t> &_modifications)
      : TofinoCPUModule(ModuleType::TofinoCPU_VectorRegisterUpdate,
                        "VectorRegisterUpdate", node),
        obj(_obj), index(_index), value_addr(_value_addr),
        modifications(_modifications) {}

  virtual void visit(EPVisitor &visitor, const EP *ep,
                     const EPNode *ep_node) const override {
    visitor.visit(ep, ep_node, this);
  }

  virtual Module *clone() const override {
    Module *cloned =
        new VectorRegisterUpdate(node, obj, index, value_addr, modifications);
    return cloned;
  }

  addr_t get_obj() const { return obj; }
  klee::ref<klee::Expr> get_index() const { return index; }
  addr_t get_value_addr() const { return value_addr; }

  const std::vector<modification_t> &get_modifications() const {
    return modifications;
  }
};

class VectorRegisterUpdateGenerator : public TofinoCPUModuleGenerator {
public:
  VectorRegisterUpdateGenerator()
      : TofinoCPUModuleGenerator(ModuleType::TofinoCPU_VectorRegisterUpdate,
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

    if (call.function_name != "vector_return") {
      return new_eps;
    }

    if (!check_placement(ep, call_node, "vector",
                         PlacementDecision::Tofino_VectorRegister)) {
      return new_eps;
    }

    klee::ref<klee::Expr> obj_expr = call.args.at("vector").expr;
    klee::ref<klee::Expr> index = call.args.at("index").expr;
    klee::ref<klee::Expr> value_addr_expr = call.args.at("value").expr;
    klee::ref<klee::Expr> value = call.args.at("value").in;

    addr_t obj = kutil::expr_addr_to_obj_addr(obj_expr);
    addr_t value_addr = kutil::expr_addr_to_obj_addr(value_addr_expr);

    klee::ref<klee::Expr> original_value =
        get_original_vector_value(ep, node, obj);
    std::vector<modification_t> changes =
        build_modifications(original_value, value);

    // Check the Ignore module.
    if (changes.empty()) {
      return new_eps;
    }

    EP *new_ep = new EP(*ep);
    new_eps.push_back(new_ep);

    Module *module =
        new VectorRegisterUpdate(node, obj, index, value_addr, changes);
    EPNode *ep_node = new EPNode(module);

    EPLeaf leaf(ep_node, node->get_next());
    new_ep->process_leaf(ep_node, {leaf});

    return new_eps;
  }
};

} // namespace tofino_cpu
} // namespace synapse
