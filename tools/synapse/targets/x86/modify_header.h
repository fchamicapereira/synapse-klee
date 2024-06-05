#pragma once

#include "x86_module.h"

namespace synapse {
namespace x86 {

class ModifyHeader : public x86Module {
private:
  addr_t chunk_addr;
  klee::ref<klee::Expr> original_chunk;
  std::vector<modification_t> changes;

public:
  ModifyHeader(const bdd::Node *node, addr_t _chunk_addr,
               klee::ref<klee::Expr> _original_chunk,
               const std::vector<modification_t> &_changes)
      : x86Module(ModuleType::x86_ModifyHeader, "ModifyHeader", node),
        chunk_addr(_chunk_addr), original_chunk(_original_chunk),
        changes(_changes) {}

  virtual void visit(EPVisitor &visitor, const EP *ep,
                     const EPNode *ep_node) const override {
    visitor.visit(ep, ep_node, this);
  }

  virtual Module *clone() const {
    ModifyHeader *cloned =
        new ModifyHeader(node, chunk_addr, original_chunk, changes);
    return cloned;
  }

  addr_t get_chunk_addr() const { return chunk_addr; }
  klee::ref<klee::Expr> chunk_borrow_from_return() const {
    return original_chunk;
  }
  const std::vector<modification_t> &get_changes() const { return changes; }
};

class ModifyHeaderGenerator : public x86ModuleGenerator {
public:
  ModifyHeaderGenerator()
      : x86ModuleGenerator(ModuleType::x86_ModifyHeader, "ModifyHeader") {}

protected:
  virtual std::vector<const EP *>
  process_node(const EP *ep, const bdd::Node *node) const override {
    std::vector<const EP *> new_eps;

    if (node->get_type() != bdd::NodeType::CALL) {
      return new_eps;
    }

    const bdd::Call *call_node = static_cast<const bdd::Call *>(node);
    const call_t &call = call_node->get_call();

    if (call.function_name != "packet_return_chunk") {
      return new_eps;
    }

    klee::ref<klee::Expr> chunk = call.args.at("the_chunk").expr;
    klee::ref<klee::Expr> current_chunk = call.args.at("the_chunk").in;
    klee::ref<klee::Expr> original_chunk = chunk_borrow_from_return(ep, node);

    addr_t chunk_addr = kutil::expr_addr_to_obj_addr(chunk);

    std::vector<modification_t> changes =
        build_modifications(original_chunk, current_chunk);

    EP *new_ep = new EP(*ep);
    new_eps.push_back(new_ep);

    if (changes.size() == 0) {
      new_ep->process_leaf(node->get_next());
      return new_eps;
    }

    Module *module =
        new ModifyHeader(node, chunk_addr, original_chunk, changes);
    EPNode *ep_node = new EPNode(module);

    EPLeaf leaf(ep_node, node->get_next());
    new_ep->process_leaf(ep_node, {leaf});

    return new_eps;
  }
};

} // namespace x86
} // namespace synapse
