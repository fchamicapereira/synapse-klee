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

    bool replicated_bdd =
        replicate_parsing_operations(ep, node, &new_bdd, &next);

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
  symbols_t get_dataplane_state(const EP *ep, const bdd::Node *node) const {
    const bdd::nodes_t &roots = ep->get_target_roots(TargetType::Tofino);
    return get_prev_symbols(node, roots);
  }

  bool replicate_parsing_operations(const EP *ep, const bdd::Node *node,
                                    bdd::BDD **new_bdd,
                                    const bdd::Node **next) const {
    std::vector<const bdd::Node *> prev_borrows =
        get_prev_functions(ep, node, {"packet_borrow_next_chunk"});

    if (prev_borrows.empty()) {
      return false;
    }

    const bdd::BDD *old_bdd = ep->get_bdd();

    bdd::BDD *bdd = new bdd::BDD(*old_bdd);
    *new_bdd = bdd;

    bdd::node_id_t &id = bdd->get_mutable_id();
    bdd::NodeManager &manager = bdd->get_mutable_manager();

    const bdd::Node *prev = node->get_prev();
    assert(prev);

    bdd::node_id_t anchor_id = prev->get_id();
    bdd::Node *anchor = bdd->get_mutable_node_by_id(anchor_id);
    bdd::Node *anchor_next = bdd->get_mutable_node_by_id(node->get_id());

    bool set_next = false;

    for (const bdd::Node *borrow : prev_borrows) {
      bdd::Node *clone = borrow->clone(manager, false);
      clone->recursive_update_ids(id);

      if (!set_next) {
        *next = clone;
        set_next = true;
      }

      switch (anchor->get_type()) {
      case bdd::NodeType::CALL:
      case bdd::NodeType::ROUTE: {
        anchor->set_next(clone);
      } break;
      case bdd::NodeType::BRANCH: {
        bdd::Branch *branch = static_cast<bdd::Branch *>(anchor);

        const bdd::Node *on_true = branch->get_on_true();
        const bdd::Node *on_false = branch->get_on_false();

        assert(on_true == anchor_next || on_false == anchor_next);

        if (on_true == anchor_next) {
          branch->set_on_true(clone);
        } else {
          branch->set_on_false(clone);
        }

      } break;
      }

      clone->set_prev(anchor);
      clone->set_next(anchor_next);
      anchor = clone;
    }

    return true;
  }
};

} // namespace tofino
} // namespace synapse
