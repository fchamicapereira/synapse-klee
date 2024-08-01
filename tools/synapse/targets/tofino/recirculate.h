#pragma once

#include "tofino_module.h"

namespace synapse {
namespace tofino {

class Recirculate : public TofinoModule {
private:
  symbols_t symbols;
  int recirc_port;

public:
  Recirculate(const bdd::Node *node, symbols_t _symbols, int _recirc_port)
      : TofinoModule(ModuleType::Tofino_Recirculate, "Recirculate", node),
        symbols(_symbols), recirc_port(_recirc_port) {}

  virtual void visit(EPVisitor &visitor, const EP *ep,
                     const EPNode *ep_node) const override {
    visitor.visit(ep, ep_node, this);
  }

  virtual Module *clone() const {
    Recirculate *cloned = new Recirculate(node, symbols, recirc_port);
    return cloned;
  }

  symbols_t get_symbols() const { return symbols; }
  int get_recirc_port() const { return recirc_port; }
};

class RecirculateGenerator : public TofinoModuleGenerator {
public:
  RecirculateGenerator()
      : TofinoModuleGenerator(ModuleType::Tofino_Recirculate, "Recirculate") {}

protected:
  virtual std::optional<speculation_t>
  speculate(const EP *ep, const bdd::Node *node,
            const Context &ctx) const override {
    // No reason to speculatively predict recirculations.
    return std::nullopt;
  }

  virtual std::vector<__generator_product_t>
  process_node(const EP *ep, const bdd::Node *node) const override {
    std::vector<__generator_product_t> products;

    const TofinoContext *tofino_ctx = get_tofino_ctx(ep);
    std::unordered_set<DS_ID> deps = tofino_ctx->get_stateful_deps(ep, node);

    // We can only recirculate if a stateful operation was implemented since the
    // last recirculation.
    if (deps.empty()) {
      return products;
    }

    int total_recirc_ports = get_total_recirc_ports(ep);

    std::vector<int> past_recirc =
        get_past_recirc_ports(ep, total_recirc_ports);

    symbols_t symbols = get_dataplane_state(ep, node);

    products = concretize_single_port_recirc(ep, node, past_recirc,
                                             total_recirc_ports, symbols);

    return products;
  }

private:
  std::vector<__generator_product_t> concretize_single_port_recirc(
      const EP *ep, const bdd::Node *node, const std::vector<int> &past_recirc,
      int total_recirc_ports, const symbols_t &symbols) const {
    std::vector<__generator_product_t> products;

    for (int recirc_port = 0; recirc_port < total_recirc_ports; recirc_port++) {
      EP *new_ep = generate_new_ep(ep, node, symbols, recirc_port, past_recirc);

      std::stringstream descr;
      descr << "port=" << recirc_port;

      products.emplace_back(new_ep, descr.str());
    }

    return products;
  }

  int get_total_recirc_ports(const EP *ep) const {
    const TofinoContext *ctx = get_tofino_ctx(ep);
    const TNA &tna = ctx->get_tna();
    const TNAProperties &properties = tna.get_properties();
    return properties.total_recirc_ports;
  }

  EP *generate_new_ep(const EP *ep, const bdd::Node *node,
                      const symbols_t &symbols, int recirc_port,
                      const std::vector<int> &past_recirc) const {
    EP *new_ep = new EP(*ep);

    Module *module = new Recirculate(node, symbols, recirc_port);
    EPNode *ep_node = new EPNode(module);

    TofinoContext *tofino_ctx = get_mutable_tofino_ctx(new_ep);
    double recirc_fraction = ep->get_active_leaf_hit_rate();

    int port_recirculations = 1;
    std::optional<int> past_recirc_port;

    for (int p : past_recirc) {
      if (p == recirc_port) {
        port_recirculations += 1;
      }

      if (!past_recirc_port.has_value()) {
        past_recirc_port = p;
      }
    }

    tofino_ctx->add_recirculated_traffic(recirc_port, port_recirculations,
                                         recirc_fraction, past_recirc_port);

    // Note that we don't point to the next BDD node, as it was not actually
    // implemented.
    // We are delegating the implementation to other platform.
    EPLeaf leaf(ep_node, node);
    new_ep->process_leaf(ep_node, {leaf}, false);

    return new_ep;
  }

  std::vector<int> get_past_recirc_ports(const EP *ep,
                                         int total_recirc_ports) const {
    const EPLeaf *active_leaf = ep->get_active_leaf();
    const EPNode *node = active_leaf->node;

    std::vector<int> past_recirc;

    while ((node = node->get_prev())) {
      const Module *module = node->get_module();

      if (!module) {
        continue;
      }

      if (module->get_type() == ModuleType::Tofino_Recirculate) {
        const Recirculate *recirc_module =
            static_cast<const Recirculate *>(module);
        int recirc_port = recirc_module->get_recirc_port();
        assert(recirc_port <= total_recirc_ports);

        past_recirc.push_back(recirc_port);
      }
    }

    return past_recirc;
  }
};

} // namespace tofino
} // namespace synapse
