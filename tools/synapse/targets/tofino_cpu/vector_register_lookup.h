#pragma once

#include "tofino_cpu_module.h"

namespace synapse {
namespace tofino_cpu {

using namespace synapse::tofino;

class VectorRegisterLookup : public TofinoCPUModule {
private:
  addr_t obj;
  klee::ref<klee::Expr> index;
  klee::ref<klee::Expr> value;

public:
  VectorRegisterLookup(const bdd::Node *node, addr_t _obj,
                       klee::ref<klee::Expr> _index,
                       klee::ref<klee::Expr> _value)
      : TofinoCPUModule(ModuleType::TofinoCPU_VectorRegisterLookup,
                        "VectorRegisterLookup", node),
        obj(_obj), index(_index), value(_value) {}

  virtual void visit(EPVisitor &visitor, const EP *ep,
                     const EPNode *ep_node) const override {
    visitor.visit(ep, ep_node, this);
  }

  virtual Module *clone() const override {
    Module *cloned = new VectorRegisterLookup(node, obj, index, value);
    return cloned;
  }

  addr_t get_obj() const { return obj; }
  klee::ref<klee::Expr> get_index() const { return index; }
  klee::ref<klee::Expr> get_value() const { return value; }
};

class VectorRegisterLookupGenerator : public TofinoCPUModuleGenerator {
public:
  VectorRegisterLookupGenerator()
      : TofinoCPUModuleGenerator(ModuleType::TofinoCPU_VectorRegisterLookup,
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

    if (!check_placement(ep, call_node, "vector",
                         PlacementDecision::Tofino_VectorRegister)) {
      return new_eps;
    }

    klee::ref<klee::Expr> vector_addr_expr = call.args.at("vector").expr;
    klee::ref<klee::Expr> index = call.args.at("index").expr;
    klee::ref<klee::Expr> value = call.extra_vars.at("borrowed_cell").second;

    addr_t vector_addr = kutil::expr_addr_to_obj_addr(vector_addr_expr);

    Module *module = new VectorRegisterLookup(node, vector_addr, index, value);
    EPNode *ep_node = new EPNode(module);

    EP *new_ep = new EP(*ep);
    new_eps.push_back(new_ep);

    EPLeaf leaf(ep_node, node->get_next());
    new_ep->process_leaf(ep_node, {leaf});

    return new_eps;
  }
};

} // namespace tofino_cpu
} // namespace synapse
