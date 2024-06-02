#pragma once

#include "x86_module.h"

namespace synapse {
namespace x86 {

class SketchExpire : public x86Module {
private:
  addr_t sketch_addr;
  klee::ref<klee::Expr> time;

public:
  SketchExpire(const bdd::Node *node, addr_t _sketch_addr,
               klee::ref<klee::Expr> _time)
      : x86Module(ModuleType::x86_SketchExpire, "SketchExpire", node),
        sketch_addr(_sketch_addr), time(_time) {}

  virtual void visit(EPVisitor &visitor, const EP *ep,
                     const EPNode *ep_node) const override {
    visitor.visit(ep, ep_node, this);
  }

  virtual Module *clone() const override {
    Module *cloned = new SketchExpire(node, sketch_addr, time);
    return cloned;
  }

  addr_t get_sketch_addr() const { return sketch_addr; }
  klee::ref<klee::Expr> get_time() const { return time; }
};

class SketchExpireGenerator : public x86ModuleGenerator {
public:
  SketchExpireGenerator()
      : x86ModuleGenerator(ModuleType::x86_SketchExpire, "SketchExpire") {}

protected:
  virtual std::vector<const EP *>
  process_node(const EP *ep, const bdd::Node *node) const override {
    std::vector<const EP *> new_eps;

    if (node->get_type() != bdd::NodeType::CALL) {
      return new_eps;
    }

    const bdd::Call *call_node = static_cast<const bdd::Call *>(node);
    const call_t &call = call_node->get_call();

    if (call.function_name != "sketch_expire") {
      return new_eps;
    }

    klee::ref<klee::Expr> sketch_addr_expr = call.args.at("sketch").expr;
    klee::ref<klee::Expr> time = call.args.at("time").expr;

    addr_t sketch_addr = kutil::expr_addr_to_obj_addr(sketch_addr_expr);

    Module *module = new SketchExpire(node, sketch_addr, time);
    EPNode *ep_node = new EPNode(module);

    EP *new_ep = new EP(*ep);
    new_eps.push_back(new_ep);

    EPLeaf leaf(ep_node, node->get_next());
    new_ep->process_leaf(ep_node, {leaf});

    return new_eps;
  }
};

} // namespace x86
} // namespace synapse
