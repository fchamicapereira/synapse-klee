#pragma once

#include "call-paths-to-bdd.h"
#include "klee-util.h"
#include "bdd-reorderer.h"

#include <unordered_map>

#include "../util.h"
#include "../log.h"

namespace synapse {

class Target;
enum class TargetType;

enum class PlacementDecision {
  Tofino_SimpleTable,
  Tofino_VectorRegister,
  Tofino_CachedTable,
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

class TargetContext {
public:
  TargetContext() {}

  virtual ~TargetContext() {}

  virtual TargetContext *clone() const = 0;
  virtual int estimate_throughput_kpps() const = 0;
};

class Context {
private:
  std::unordered_map<addr_t, bdd::map_config_t> map_configs;
  std::unordered_map<addr_t, bdd::vector_config_t> vector_configs;
  std::unordered_map<addr_t, bdd::dchain_config_t> dchain_configs;
  std::unordered_map<addr_t, bdd::sketch_config_t> sketch_configs;
  std::unordered_map<addr_t, bdd::cht_config_t> cht_configs;
  std::vector<map_coalescing_data_t> coalescing_candidates;
  std::optional<expiration_data_t> expiration_data;
  std::unordered_map<addr_t, PlacementDecision> placement_decisions;
  std::unordered_map<TargetType, TargetContext *> target_ctxs;

public:
  Context(const bdd::BDD *bdd, const std::vector<const Target *> &targets);
  Context(const Context &other);
  Context(Context &&other);

  ~Context();

  Context &operator=(const Context &other);

  const bdd::map_config_t &get_map_config(addr_t addr) const;
  const bdd::vector_config_t &get_vector_config(addr_t addr) const;
  const bdd::dchain_config_t &get_dchain_config(addr_t addr) const;
  const bdd::sketch_config_t &get_sketch_config(addr_t addr) const;
  const bdd::cht_config_t &get_cht_config(addr_t addr) const;

  std::optional<map_coalescing_data_t> get_coalescing_data(addr_t obj) const;
  const std::optional<expiration_data_t> &get_expiration_data() const;

  const std::unordered_map<addr_t, PlacementDecision> &get_placements() const;

  template <class TCtx> const TCtx *get_target_ctx() const;
  template <class TCtx> TCtx *get_mutable_target_ctx();

  void save_placement(addr_t obj, PlacementDecision decision);
  bool has_placement(addr_t obj) const;
  bool check_placement(addr_t obj, PlacementDecision decision) const;
  bool can_place(addr_t obj, PlacementDecision decision) const;
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
