#pragma once

#include "call-paths-to-bdd.h"
#include "klee-util.h"
#include "bdd-reorderer.h"

#include <unordered_map>

#include "../util.h"
#include "../log.h"
#include "../profiler.h"
#include "../execution_plan/node.h"
#include "target.h"

namespace synapse {

class EP;

enum class PlacementDecision {
  Tofino_SimpleTable,
  Tofino_VectorRegister,
  Tofino_TTLCachedTable,
  TofinoCPU_Map,
  TofinoCPU_Vector,
  TofinoCPU_Dchain,
  TofinoCPU_Cht,
  TofinoCPU_Sketch,
  x86_Map,
  x86_Vector,
  x86_Dchain,
  x86_Cht,
  x86_Sketch,
};

std::ostream &operator<<(std::ostream &os, PlacementDecision decision);

struct expiration_data_t {
  time_ns_t expiration_time;
  symbol_t number_of_freed_flows;
};

struct speculation_t;

class TargetContext {
public:
  TargetContext() {}

  virtual ~TargetContext() {}

  virtual TargetContext *clone() const = 0;
  virtual uint64_t estimate_throughput_pps() const = 0;
};

class Context {
private:
  std::shared_ptr<Profiler> profiler;
  bool profiler_mutations_allowed;

  std::unordered_map<addr_t, bdd::map_config_t> map_configs;
  std::unordered_map<addr_t, bdd::vector_config_t> vector_configs;
  std::unordered_map<addr_t, bdd::dchain_config_t> dchain_configs;
  std::unordered_map<addr_t, bdd::sketch_config_t> sketch_configs;
  std::unordered_map<addr_t, bdd::cht_config_t> cht_configs;
  std::vector<map_coalescing_data_t> coalescing_candidates;
  std::optional<expiration_data_t> expiration_data;
  std::unordered_map<addr_t, PlacementDecision> placement_decisions;
  std::unordered_map<TargetType, TargetContext *> target_ctxs;
  std::unordered_map<TargetType, double> traffic_fraction_per_target;
  std::unordered_map<ep_node_id_t, constraints_t> constraints_per_node;
  uint64_t throughput_estimate_pps;
  uint64_t throughput_speculation_pps;

public:
  Context(const bdd::BDD *bdd, const targets_t &targets,
          const TargetType initial_target, std::shared_ptr<Profiler> profiler);
  Context(const Context &other);
  Context(Context &&other);

  ~Context();

  Context &operator=(const Context &other);

  const Profiler *get_profiler() const;
  const bdd::map_config_t &get_map_config(addr_t addr) const;
  const bdd::vector_config_t &get_vector_config(addr_t addr) const;
  const bdd::dchain_config_t &get_dchain_config(addr_t addr) const;
  const bdd::sketch_config_t &get_sketch_config(addr_t addr) const;
  const bdd::cht_config_t &get_cht_config(addr_t addr) const;
  std::optional<map_coalescing_data_t> get_coalescing_data(addr_t obj) const;
  const std::optional<expiration_data_t> &get_expiration_data() const;
  const std::unordered_map<addr_t, PlacementDecision> &get_placements() const;
  const std::unordered_map<TargetType, double> &get_traffic_fractions() const;

  template <class TCtx> const TCtx *get_target_ctx() const;
  template <class TCtx> TCtx *get_mutable_target_ctx();

  void save_placement(addr_t obj, PlacementDecision decision);
  bool has_placement(addr_t obj) const;
  bool check_placement(addr_t obj, PlacementDecision decision) const;
  bool can_place(addr_t obj, PlacementDecision decision) const;

  void update_constraints_per_node(ep_node_id_t node,
                                   const constraints_t &constraints);
  constraints_t get_node_constraints(const EPNode *node) const;

  void update_traffic_fractions(const EPNode *new_node);
  void update_traffic_fractions(TargetType old_target, TargetType new_target,
                                double fraction);

  void update_throughput_estimates(const EP *ep);
  uint64_t get_throughput_estimate_pps() const;
  uint64_t get_throughput_speculation_pps() const;

  void add_hit_rate_estimation(const constraints_t &constraints,
                               klee::ref<klee::Expr> new_constraint,
                               double estimation_rel);
  void remove_hit_rate_node(const constraints_t &constraints);
  void scale_profiler(const constraints_t &constraints, double factor);

  void log_debug() const;

private:
  void update_throughput_speculation(const EP *ep);
  void update_throughput_estimate();
  void allow_profiler_mutation();

  struct node_speculation_t;

  void
  print_speculations(const EP *ep,
                     const std::vector<node_speculation_t> &node_speculations,
                     const speculation_t &speculation) const;

  node_speculation_t
  get_best_speculation(const EP *ep, const bdd::Node *node,
                       const targets_t &targets, TargetType current_target,
                       const speculation_t &current_speculation) const;

  speculation_t peek_speculation_for_future_nodes(
      const speculation_t &base_speculation, const EP *ep,
      const bdd::Node *anchor, bdd::nodes_t future_nodes,
      const targets_t &targets, TargetType current_target) const;

  bool is_better_speculation(const speculation_t &old_speculation,
                             const speculation_t &new_speculation, const EP *ep,
                             const bdd::Node *node, const targets_t &targets,
                             TargetType current_target) const;
};

#define EXPLICIT_TARGET_CONTEXT_INSTANTIATION(NS, TCTX)                        \
  namespace NS {                                                               \
  class TCTX;                                                                  \
  }                                                                            \
  template <> const NS::TCTX *Context::get_target_ctx<NS::TCTX>() const;       \
  template <> NS::TCTX *Context::get_mutable_target_ctx<NS::TCTX>();

EXPLICIT_TARGET_CONTEXT_INSTANTIATION(tofino, TofinoContext)
EXPLICIT_TARGET_CONTEXT_INSTANTIATION(tofino_cpu, TofinoCPUContext)
EXPLICIT_TARGET_CONTEXT_INSTANTIATION(x86, x86Context)

} // namespace synapse
