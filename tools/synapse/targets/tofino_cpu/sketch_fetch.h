#pragma once

#include "tofino_cpu_module.h"

namespace synapse {
namespace tofino_cpu {

class SketchFetch : public TofinoCPUModule {
private:
  addr_t sketch_addr;
  symbol_t overflow;

public:
  SketchFetch(const bdd::Node *node, addr_t _sketch_addr, symbol_t _overflow)
      : TofinoCPUModule(ModuleType::TofinoCPU_SketchFetch, "SketchFetch", node),
        sketch_addr(_sketch_addr), overflow(_overflow) {}

  virtual void visit(EPVisitor &visitor, const EP *ep,
                     const EPNode *ep_node) const override {
    visitor.visit(ep, ep_node, this);
  }

  virtual Module *clone() const override {
    Module *cloned = new SketchFetch(node, sketch_addr, overflow);
    return cloned;
  }

  addr_t get_sketch_addr() const { return sketch_addr; }
  const symbol_t &get_overflow() const { return overflow; }
};

class SketchFetchGenerator : public TofinoCPUModuleGenerator {
public:
  SketchFetchGenerator()
      : TofinoCPUModuleGenerator(ModuleType::TofinoCPU_SketchFetch,
                                 "SketchFetch") {}

protected:
  virtual std::vector<const EP *>
  process_node(const EP *ep, const bdd::Node *node) const override {
    std::vector<const EP *> new_eps;

    if (node->get_type() != bdd::NodeType::CALL) {
      return new_eps;
    }

    const bdd::Call *call_node = static_cast<const bdd::Call *>(node);
    const call_t &call = call_node->get_call();

    if (call.function_name != "sketch_fetch") {
      return new_eps;
    }

    if (!can_place(ep, call_node, "sketch",
                   PlacementDecision::TofinoCPU_Sketch)) {
      return new_eps;
    }

    klee::ref<klee::Expr> sketch_addr_expr = call.args.at("sketch").expr;
    addr_t sketch_addr = kutil::expr_addr_to_obj_addr(sketch_addr_expr);

    symbols_t symbols = call_node->get_locally_generated_symbols();
    symbol_t overflow;
    bool found = get_symbol(symbols, "overflow", overflow);
    assert(found && "Symbol overflow not found");

    Module *module = new SketchFetch(node, sketch_addr, overflow);
    EPNode *ep_node = new EPNode(module);

    EP *new_ep = new EP(*ep);
    new_eps.push_back(new_ep);

    EPLeaf leaf(ep_node, node->get_next());
    new_ep->process_leaf(ep_node, {leaf});

    place(new_ep, sketch_addr, PlacementDecision::TofinoCPU_Sketch);

    return new_eps;
  }
};

} // namespace tofino_cpu
} // namespace synapse
