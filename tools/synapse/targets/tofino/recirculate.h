#pragma once

#include "tofino_module.h"

#define MAX_RECIRCULATIONS_PER_PACKET 3

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
            const constraints_t &current_speculative_constraints,
            const Context &current_speculative_ctx) const override {
    // No reason to speculatively predict recirculations.
    return std::nullopt;
  }

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

    int total_recirc_ports = get_total_recirc_ports(ep);

    std::unordered_map<int, int> total_past_recirc =
        get_total_past_recirc_per_port(ep, total_recirc_ports);

    symbols_t symbols = get_dataplane_state(ep, node);

    generate_eps_single_port_recirc(ep, node, total_past_recirc,
                                    total_recirc_ports, symbols, new_eps);

    return new_eps;
  }

private:
  void generate_eps_single_port_recirc(
      const EP *ep, const bdd::Node *node,
      const std::unordered_map<int, int> &total_past_recirc,
      int total_recirc_ports, const symbols_t &symbols,
      std::vector<const EP *> &new_eps) const {
    for (int recirc_port = 0; recirc_port < total_recirc_ports; recirc_port++) {
      if (total_past_recirc.find(recirc_port) != total_past_recirc.end() &&
          total_past_recirc.at(recirc_port) == MAX_RECIRCULATIONS_PER_PACKET) {
        continue;
      }

      const EP *new_ep =
          generate_new_ep(ep, node, symbols, recirc_port, total_past_recirc);
      new_eps.push_back(new_ep);
    }
  }

  int get_total_recirc_ports(const EP *ep) const {
    const TofinoContext *ctx = get_tofino_ctx(ep);
    const TNA &tna = ctx->get_tna();
    const TNAProperties &properties = tna.get_properties();
    return properties.total_recirc_ports;
  }

  const EP *
  generate_new_ep(const EP *ep, const bdd::Node *node, const symbols_t &symbols,
                  int recirc_port,
                  const std::unordered_map<int, int> &total_past_recirc) const {
    EP *new_ep = new EP(*ep);

    Module *module = new Recirculate(node, symbols, recirc_port);
    EPNode *ep_node = new EPNode(module);

    // Note that we don't point to the next BDD node, as it was not actually
    // implemented.
    // We are delegating the implementation to other platform.
    EPLeaf leaf(ep_node, node);
    new_ep->process_leaf(ep_node, {leaf}, false);

    Context &ctx = new_ep->get_mutable_ctx();
    TofinoContext *tofino_ctx = get_mutable_tofino_ctx(new_ep);
    float recirc_fraction = ep->get_active_leaf_hit_rate();

    int total_recirculations = 0;
    for (auto &[recirc_port, recircs] : total_past_recirc)
      total_recirculations += recircs;
    total_recirculations += 1;

    int port_recirculations = 0;
    if (total_past_recirc.find(recirc_port) != total_past_recirc.end())
      port_recirculations = total_past_recirc.at(recirc_port);
    port_recirculations += 1;

    tofino_ctx->add_recirculated_traffic(recirc_port, port_recirculations,
                                         total_recirculations, recirc_fraction);
    ctx.update_throughput_estimates(new_ep);

    return new_ep;
  }

  std::unordered_map<int, int>
  get_total_past_recirc_per_port(const EP *ep, int total_recirc_ports) const {
    const EPLeaf *active_leaf = ep->get_active_leaf();
    const EPNode *node = active_leaf->node;

    std::unordered_map<int, int> past_recirc_per_port;

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

        if (past_recirc_per_port.find(recirc_port) ==
            past_recirc_per_port.end()) {
          past_recirc_per_port[recirc_port] = 0;
        }

        past_recirc_per_port[recirc_port]++;
      }
    }

    return past_recirc_per_port;
  }
};

} // namespace tofino
} // namespace synapse
