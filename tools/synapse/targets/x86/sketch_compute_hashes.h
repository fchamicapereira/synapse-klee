#pragma once

#include "x86_module.h"

namespace synapse {
namespace x86 {

class SketchComputeHashes : public x86Module {
private:
  addr_t sketch_addr;
  klee::ref<klee::Expr> key;

public:
  SketchComputeHashes(const bdd::Node *node, addr_t _sketch_addr,
                      klee::ref<klee::Expr> _key)
      : x86Module(ModuleType::x86_SketchComputeHashes, "SketchComputeHashes",
                  node),
        sketch_addr(_sketch_addr), key(_key) {}

  virtual void visit(EPVisitor &visitor, const EP *ep,
                     const EPNode *ep_node) const override {
    visitor.visit(ep, ep_node, this);
  }

  virtual Module *clone() const override {
    Module *cloned = new SketchComputeHashes(node, sketch_addr, key);
    return cloned;
  }

  addr_t get_sketch_addr() const { return sketch_addr; }
  klee::ref<klee::Expr> get_key() const { return key; }
};

class SketchComputeHashesGenerator : public x86ModuleGenerator {
public:
  SketchComputeHashesGenerator()
      : x86ModuleGenerator(ModuleType::x86_SketchComputeHashes,
                           "SketchComputeHashes") {}

protected:
  virtual std::vector<const EP *>
  process_node(const EP *ep, const bdd::Node *node) const override {
    std::vector<const EP *> new_eps;

    if (node->get_type() != bdd::NodeType::CALL) {
      return new_eps;
    }

    const bdd::Call *call_node = static_cast<const bdd::Call *>(node);
    const call_t &call = call_node->get_call();

    if (call.function_name != "sketch_compute_hashes") {
      return new_eps;
    }

    if (!can_place(ep, call_node, "sketch", PlacementDecision::x86Sketch)) {
      return new_eps;
    }

    klee::ref<klee::Expr> sketch_addr_expr = call.args.at("sketch").expr;
    klee::ref<klee::Expr> key = call.args.at("key").expr;

    addr_t sketch_addr = kutil::expr_addr_to_obj_addr(sketch_addr_expr);

    Module *module = new SketchComputeHashes(node, sketch_addr, key);
    EPNode *ep_node = new EPNode(module);

    EP *new_ep = new EP(*ep);
    new_eps.push_back(new_ep);

    EPLeaf leaf(ep_node, node->get_next());
    new_ep->process_leaf(ep_node, {leaf});

    place(new_ep, sketch_addr, PlacementDecision::x86Sketch);

    return new_eps;
  }
};

} // namespace x86
} // namespace synapse
