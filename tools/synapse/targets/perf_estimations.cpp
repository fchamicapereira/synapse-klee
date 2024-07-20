#include "context.h"
#include "target.h"
#include "targets.h"
#include "module_generator.h"

#include "klee-util.h"

namespace synapse {

struct Context::node_speculation_t {
  const bdd::Node *node;
  TargetType target;
  std::string module_name;
  speculation_t speculation;
  uint64_t pps;
};

static uint64_t estimate_throughput_pps_from_ctx(const Context &ctx) {
  uint64_t estimation_pps = 0;

  const std::unordered_map<TargetType, double> &traffic_fractions =
      ctx.get_traffic_fractions();

  for (const auto &[target, traffic_fraction] : traffic_fractions) {
    const TargetContext *target_ctx = nullptr;

    switch (target) {
    case TargetType::Tofino: {
      target_ctx = ctx.get_target_ctx<tofino::TofinoContext>();
    } break;
    case TargetType::TofinoCPU: {
      target_ctx = ctx.get_target_ctx<tofino_cpu::TofinoCPUContext>();
    } break;
    case TargetType::x86: {
      target_ctx = ctx.get_target_ctx<x86::x86Context>();
    } break;
    }

    uint64_t target_estimation_pps = target_ctx->estimate_throughput_pps();
    estimation_pps += target_estimation_pps * traffic_fraction;
  }

  return estimation_pps;
}

void Context::update_throughput_estimate() {
  throughput_estimate_pps = estimate_throughput_pps_from_ctx(*this);
}

struct speculative_data_t : public bdd::cookie_t {
  constraints_t constraints;
  std::unordered_map<bdd::node_id_t, klee::ref<klee::Expr>> pending_constraints;

  speculative_data_t(const constraints_t &_constraints)
      : constraints(_constraints) {}

  speculative_data_t(const speculative_data_t &other)
      : constraints(other.constraints),
        pending_constraints(other.pending_constraints) {}

  virtual speculative_data_t *clone() const {
    return new speculative_data_t(*this);
  }
};

void Context::print_speculations(
    const EP *ep,
    const std::vector<Context::node_speculation_t> &node_speculations,
    const speculation_t &speculation) const {
  std::cerr << "\n";
  std::cerr << "===============";
  std::cerr << "SPECULATION EP " << ep->get_id();
  std::cerr << "===============";
  std::cerr << "\n";

  std::cerr << "EP: " << ep->get_id() << "\n";

  for (const Context::node_speculation_t &node_speculation :
       node_speculations) {
    std::cerr << "-----------------------\n";
    std::cerr << "* Node:       " << node_speculation.node->dump(true, true)
              << "\n";
    std::cerr << "  Target:     " << node_speculation.target << "\n";
    std::cerr << "  Module:     " << node_speculation.module_name << "\n";
    std::cerr << "  Estimation: " << throughput2str(node_speculation.pps, "pps")
              << "\n";
    std::cerr << "  Traffic:    \n";
    for (auto tf : node_speculation.speculation.ctx.get_traffic_fractions()) {
      std::cerr << "    " << std::setfill('0') << std::setw(7) << std::fixed
                << std::setprecision(5) << tf.second;
      std::cerr << " [" << tf.first << "]\n";
    }
    std::cerr << "  Skip: ";
    for (bdd::node_id_t skip : node_speculation.speculation.skip) {
      std::cerr << skip << " ";
    }
    std::cerr << "\n";
  }

  uint64_t final_pps = estimate_throughput_pps_from_ctx(speculation.ctx);

  std::cerr << "-----------------------\n";

  std::cerr << "\n";
  std::cerr << "Final estimation: " << throughput2str(final_pps, "pps") << "\n";
  std::cerr << "Traffic:    \n";
  for (auto tf : speculation.ctx.get_traffic_fractions()) {
    std::cerr << "  " << std::setfill('0') << std::setw(7) << std::fixed
              << std::setprecision(5) << tf.second;
    std::cerr << " [" << tf.first << "]\n";
  }
  std::cerr << "\n";

  std::cerr << "========================================\n";
}

speculation_t Context::peek_speculation_for_future_nodes(
    const speculation_t &base_speculation, const EP *ep,
    const bdd::Node *anchor, bdd::nodes_t future_nodes,
    const std::vector<const Target *> &targets,
    TargetType current_target) const {
  future_nodes.erase(anchor->get_id());

  if (future_nodes.empty()) {
    return base_speculation;
  }

  speculation_t speculation = base_speculation;

  std::vector<Context::node_speculation_t> nodes_speculations;

  anchor->visit_nodes([this, &speculation, &nodes_speculations, &targets,
                       current_target, ep,
                       &future_nodes](const bdd::Node *node) {
    if (future_nodes.empty()) {
      return bdd::NodeVisitAction::STOP;
    }

    auto found_it = future_nodes.find(node->get_id());
    if (found_it == future_nodes.end()) {
      return bdd::NodeVisitAction::VISIT_CHILDREN;
    }

    future_nodes.erase(node->get_id());

    Context::node_speculation_t node_speculation =
        get_best_speculation(ep, node, targets, current_target, speculation);
    nodes_speculations.push_back(node_speculation);
    speculation = node_speculation.speculation;

    if (speculation.next_target.has_value() &&
        speculation.next_target != current_target) {
      return bdd::NodeVisitAction::SKIP_CHILDREN;
    }

    return bdd::NodeVisitAction::VISIT_CHILDREN;
  });

  return speculation;
}

static bdd::nodes_t filter_away_nodes(const bdd::nodes_t &nodes,
                                      const bdd::nodes_t &filter) {
  bdd::nodes_t result;

  for (bdd::node_id_t node_id : nodes) {
    if (filter.find(node_id) == filter.end()) {
      result.insert(node_id);
    }
  }

  return result;
}

// Compare the performance of an old speculation if it were subjected to the
// nodes ignored by the new speculation, and vise versa.
bool Context::is_better_speculation(const speculation_t &old_speculation,
                                    const speculation_t &new_speculation,
                                    const EP *ep, const bdd::Node *node,
                                    const std::vector<const Target *> &targets,
                                    TargetType current_target) const {
  bdd::nodes_t old_future_nodes =
      filter_away_nodes(new_speculation.skip, old_speculation.skip);
  bdd::nodes_t new_future_nodes =
      filter_away_nodes(old_speculation.skip, new_speculation.skip);

  speculation_t peek_old = peek_speculation_for_future_nodes(
      old_speculation, ep, node, old_future_nodes, targets, current_target);

  speculation_t peek_new = peek_speculation_for_future_nodes(
      new_speculation, ep, node, new_future_nodes, targets, current_target);

  uint64_t old_pps = estimate_throughput_pps_from_ctx(peek_old.ctx);
  uint64_t new_pps = estimate_throughput_pps_from_ctx(peek_new.ctx);

  return new_pps > old_pps;
}

Context::node_speculation_t
Context::get_best_speculation(const EP *ep, const bdd::Node *node,
                              const std::vector<const Target *> &targets,
                              TargetType current_target,
                              const speculation_t &current_speculation) const {
  std::optional<speculation_t> best_local_speculation;
  std::string best_local_module;

  for (const Target *target : targets) {
    if (target->type != current_target) {
      continue;
    }

    for (const ModuleGenerator *modgen : target->module_generators) {
      std::optional<speculation_t> new_speculation =
          modgen->speculate(ep, node, current_speculation.ctx);

      if (!new_speculation.has_value()) {
        continue;
      }

      new_speculation->skip.insert(current_speculation.skip.begin(),
                                   current_speculation.skip.end());

      if (!best_local_speculation.has_value()) {
        best_local_speculation = new_speculation;
        best_local_module = modgen->get_name();
        continue;
      }

      // bool is_better =
      //     is_better_speculation(*best_local_speculation, *new_speculation,
      //     ep,
      //                           node, targets, current_target);
      bool is_better =
          estimate_throughput_pps_from_ctx(new_speculation->ctx) >
          estimate_throughput_pps_from_ctx(best_local_speculation->ctx);

      if (is_better) {
        best_local_speculation = new_speculation;
        best_local_module = modgen->get_name();
      }
    }
  }

  if (!best_local_speculation.has_value()) {
    Log::err() << "No module to speculative execute\n";
    Log::err() << "Target: " << current_target << "\n";
    Log::err() << "Node:   " << node->dump(true) << "\n";
    exit(1);
  }

  Context::node_speculation_t report = {
      .node = node,
      .target = current_target,
      .module_name = best_local_module,
      .speculation = *best_local_speculation,
      .pps = estimate_throughput_pps_from_ctx(best_local_speculation->ctx),
  };

  return report;
}

void Context::update_throughput_speculation(const EP *ep) {
  const std::vector<EPLeaf> &leaves = ep->get_leaves();
  const std::vector<const Target *> &targets = ep->get_targets();

  speculation_t speculation(*this);
  std::vector<Context::node_speculation_t> nodes_speculations;

  for (const EPLeaf &leaf : leaves) {
    const bdd::Node *node = leaf.next;

    if (!node) {
      continue;
    }

    TargetType current_target;
    constraints_t constraints;

    if (leaf.node) {
      const Module *module = leaf.node->get_module();
      current_target = module->get_next_target();
      constraints = get_node_constraints(leaf.node);
    } else {
      current_target = ep->get_current_platform();
    }

    node->visit_nodes([this, &speculation, &targets, &nodes_speculations,
                       current_target, ep](const bdd::Node *node) {
      if (speculation.skip.find(node->get_id()) != speculation.skip.end()) {
        return bdd::NodeVisitAction::VISIT_CHILDREN;
      }

      Context::node_speculation_t node_speculation =
          get_best_speculation(ep, node, targets, current_target, speculation);
      nodes_speculations.push_back(node_speculation);
      speculation = node_speculation.speculation;

      if (speculation.next_target.has_value() &&
          speculation.next_target != current_target) {
        return bdd::NodeVisitAction::SKIP_CHILDREN;
      }

      return bdd::NodeVisitAction::VISIT_CHILDREN;
    });
  }

  // if (ep->get_id() == 286) {
  //   print_speculations(ep, nodes_speculations, speculation);
  //   bdd::BDDVisualizer::visualize(ep->get_bdd(), true);
  // }

  throughput_speculation_pps =
      estimate_throughput_pps_from_ctx(speculation.ctx);
}

void Context::update_throughput_estimates(const EP *ep) {
  update_throughput_estimate();
  update_throughput_speculation(ep);
}

uint64_t Context::get_throughput_estimate_pps() const {
  return throughput_estimate_pps;
}

uint64_t Context::get_throughput_speculation_pps() const {
  return throughput_speculation_pps;
}

} // namespace synapse