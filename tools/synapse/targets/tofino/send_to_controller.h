#pragma once

#include "tofino_module.h"

namespace synapse {
namespace tofino {

class SendToController : public TofinoModule {
private:
  symbols_t symbols;

public:
  SendToController(const bdd::Node *node, symbols_t _symbols)
      : TofinoModule(ModuleType::Tofino_SendToController, TargetType::TofinoCPU,
                     "SendToController", node),
        symbols(_symbols) {}

  virtual void visit(EPVisitor &visitor, const EP *ep,
                     const EPNode *ep_node) const override {
    visitor.visit(ep, ep_node, this);
  }

  virtual Module *clone() const {
    SendToController *cloned = new SendToController(node, symbols);
    return cloned;
  }

  symbols_t get_symbols() const { return symbols; }
};

class SendToControllerGenerator : public TofinoModuleGenerator {
public:
  SendToControllerGenerator()
      : TofinoModuleGenerator(ModuleType::Tofino_SendToController,
                              "SendToController") {}

protected:
  virtual std::vector<const EP *>
  process_node(const EP *ep, const bdd::Node *node) const override {
    std::vector<const EP *> new_eps;

    // We can always send to the controller, at any point in time.
    EP *new_ep = new EP(*ep);
    new_eps.push_back(new_ep);

    symbols_t symbols = get_dataplane_state(ep, node);

    Module *module = new SendToController(node, symbols);
    EPNode *ep_node = new EPNode(module);

    // Now we need to replicate the parsing operations that were done before.
    bdd::BDD *new_bdd = nullptr;
    const bdd::Node *next = node;

    bool replicated_bdd = replicate_hdr_parsing_ops(ep, node, new_bdd, next);

    // Note that we don't point to the next BDD node, as it was not actually
    // implemented.
    // We are delegating the implementation to other platform.
    EPLeaf leaf(ep_node, next);
    new_ep->process_leaf(ep_node, {leaf}, false);

    if (replicated_bdd) {
      new_ep->replace_bdd(new_bdd);
    }

    // FIXME: missing custom packet parsing for the SyNAPSE header.

    return new_eps;
  }

private:
  bool replicate_hdr_parsing_ops(const EP *ep, const bdd::Node *node,
                                 bdd::BDD *&new_bdd,
                                 const bdd::Node *&next) const {
    std::vector<const bdd::Node *> prev_borrows =
        get_prev_functions(ep, node, {"packet_borrow_next_chunk"});

    if (prev_borrows.empty()) {
      return false;
    }

    const bdd::BDD *old_bdd = ep->get_bdd();

    new_bdd = new bdd::BDD(*old_bdd);
    bdd::Node *new_next;
    add_non_branch_nodes_to_bdd(ep, new_bdd, node, prev_borrows, new_next);

    next = new_next;

    return true;
  }
};

} // namespace tofino
} // namespace synapse
