#pragma once

#include "tofino_module.h"

namespace synapse {
namespace tofino {

class Recirculate : public TofinoModule {
private:
  symbols_t symbols;
  int recirculation_port;

public:
  Recirculate(const bdd::Node *node, symbols_t _symbols,
              int _recirculation_port)
      : TofinoModule(ModuleType::Tofino_Recirculate, "Recirculate", node),
        symbols(_symbols), recirculation_port(_recirculation_port) {}

  virtual void visit(EPVisitor &visitor, const EP *ep,
                     const EPNode *ep_node) const override {
    visitor.visit(ep, ep_node, this);
  }

  virtual Module *clone() const {
    Recirculate *cloned = new Recirculate(node, symbols, recirculation_port);
    return cloned;
  }

  symbols_t get_symbols() const { return symbols; }
  int get_recirculation_port() const { return recirculation_port; }
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

    symbols_t symbols = get_dataplane_state(ep, node);
    int total_recirculation_ports = get_total_recirculation_ports(ep);

    for (int i = 0; i < total_recirculation_ports; i++) {
      const EP *new_ep = generate_new_ep(ep, node, symbols, i);
      new_eps.push_back(new_ep);
    }

    return new_eps;
  }

private:
  int get_total_recirculation_ports(const EP *ep) const {
    const TofinoContext *ctx = get_tofino_ctx(ep);
    const TNA &tna = ctx->get_tna();
    const TNAProperties &properties = tna.get_properties();
    return properties.total_recirculation_ports;
  }

  const EP *generate_new_ep(const EP *ep, const bdd::Node *node,
                            const symbols_t &symbols,
                            int recirculation_port) const {
    EP *new_ep = new EP(*ep);

    Module *module = new Recirculate(node, symbols, recirculation_port);
    EPNode *ep_node = new EPNode(module);

    // Note that we don't point to the next BDD node, as it was not actually
    // implemented.
    // We are delegating the implementation to other platform.
    EPLeaf leaf(ep_node, node);
    new_ep->process_leaf(ep_node, {leaf}, false);

    update_fraction_of_traffic_recirculated_metric(recirculation_port, new_ep);

    return new_ep;
  }

  void update_fraction_of_traffic_recirculated_metric(int recirculation_port,
                                                      EP *ep) const {
    TofinoContext *ctx = get_mutable_tofino_ctx(ep);
    float fraction_recirculated = ep->get_active_leaf_hit_rate();

    if (is_surplus(ep)) {
      ctx->inc_recirculation_surplus(recirculation_port, fraction_recirculated);
    } else {
      ctx->inc_fraction_of_traffic_recirculated(recirculation_port,
                                                fraction_recirculated);
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
