#pragma once

#include "x86_module.h"

namespace synapse {
namespace x86 {

class SketchTouchBuckets : public x86Module {
private:
  addr_t sketch_addr;
  klee::ref<klee::Expr> time;
  symbol_t success;

public:
  SketchTouchBuckets(const bdd::Node *node, addr_t _sketch_addr,
                     klee::ref<klee::Expr> _time, symbol_t _success)
      : x86Module(ModuleType::x86_SketchTouchBuckets, "SketchTouchBuckets",
                  node),
        sketch_addr(_sketch_addr), time(_time), success(_success) {}

  virtual void visit(EPVisitor &visitor, const EP *ep,
                     const EPNode *ep_node) const override {
    visitor.visit(ep, ep_node, this);
  }

  virtual Module *clone() const override {
    Module *cloned = new SketchTouchBuckets(node, sketch_addr, time, success);
    return cloned;
  }

  addr_t get_sketch_addr() const { return sketch_addr; }
  klee::ref<klee::Expr> get_time() const { return time; }
  const symbol_t &get_success() const { return success; }
};

class SketchTouchBucketsGenerator : public x86ModuleGenerator {
public:
  SketchTouchBucketsGenerator()
      : x86ModuleGenerator(ModuleType::x86_SketchTouchBuckets,
                           "SketchTouchBuckets") {}

protected:
  virtual std::vector<const EP *>
  process_node(const EP *ep, const bdd::Node *node) const override {
    std::vector<const EP *> new_eps;

    if (node->get_type() != bdd::NodeType::CALL) {
      return new_eps;
    }

    const bdd::Call *call_node = static_cast<const bdd::Call *>(node);
    const call_t &call = call_node->get_call();

    if (call.function_name != "sketch_touch_buckets") {
      return new_eps;
    }

    klee::ref<klee::Expr> sketch_addr_expr = call.args.at("sketch").expr;
    klee::ref<klee::Expr> time = call.args.at("time").expr;

    addr_t sketch_addr = kutil::expr_addr_to_obj_addr(sketch_addr_expr);

    symbols_t symbols = call_node->get_locally_generated_symbols();
    symbol_t success;
    bool found = get_symbol(symbols, "success", success);
    assert(found && "Symbol success not found");

    Module *module = new SketchTouchBuckets(node, sketch_addr, time, success);
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
