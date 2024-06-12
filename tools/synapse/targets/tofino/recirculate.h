#pragma once

#include "tofino_module.h"

namespace synapse {
namespace tofino {

class Recirculate : public TofinoModule {
private:
  symbols_t symbols;

public:
  Recirculate(const bdd::Node *node, symbols_t _symbols)
      : TofinoModule(ModuleType::Tofino_Recirculate, "Recirculate", node),
        symbols(_symbols) {}

  virtual void visit(EPVisitor &visitor, const EP *ep,
                     const EPNode *ep_node) const override {
    visitor.visit(ep, ep_node, this);
  }

  virtual Module *clone() const {
    Recirculate *cloned = new Recirculate(node, symbols);
    return cloned;
  }

  symbols_t get_symbols() const { return symbols; }
};

class RecirculateGenerator : public TofinoModuleGenerator {
public:
  RecirculateGenerator()
      : TofinoModuleGenerator(ModuleType::Tofino_Recirculate, "Recirculate") {}

protected:
  virtual std::vector<const EP *>
  process_node(const EP *ep, const bdd::Node *node) const override {
    std::vector<const EP *> new_eps;

    // We can always recirculate, with a small exception: if we have just
    // recirculated, and did nothing meanwhile.

    const EPLeaf *active_leaf = ep->get_active_leaf();
    const EPNode *active_node = active_leaf->node;

    if (!active_node) {
      // Root leaf.
      return new_eps;
    }

    const Module *active_module = active_node->get_module();

    if (active_module->get_type() == ModuleType::Tofino_Recirculate) {
      return new_eps;
    }

    EP *new_ep = new EP(*ep);
    new_eps.push_back(new_ep);

    symbols_t symbols = get_dataplane_state(ep, node);

    Module *module = new Recirculate(node, symbols);
    EPNode *ep_node = new EPNode(module);

    // Note that we don't point to the next BDD node, as it was not actually
    // implemented.
    // We are delegating the implementation to other platform.
    EPLeaf leaf(ep_node, node);
    new_ep->process_leaf(ep_node, {leaf}, false);

    update_fraction_of_traffic_recirculated_metric(new_ep);

    return new_eps;
  }

private:
  void update_fraction_of_traffic_recirculated_metric(EP *ep) const {
    TofinoContext *ctx = get_mutable_tofino_ctx(ep);
    float fraction_recirculated = ep->get_active_leaf_hit_rate();

    if (is_surplus(ep)) {
      ctx->inc_recirculation_surplus(fraction_recirculated);
    } else {
      ctx->inc_fraction_of_traffic_recirculated(fraction_recirculated);
    }
  }

  bool is_surplus(EP *ep) const {
    const EPLeaf *active_leaf = ep->get_active_leaf();
    const EPNode *node = active_leaf->node;

    while ((node = node->get_prev())) {
      const Module *module = node->get_module();

      if (!module) {
        break;
      }

      if (module->get_type() == ModuleType::Tofino_Recirculate) {
        return true;
      }
    }

    return false;
  }
};

} // namespace tofino
} // namespace synapse
