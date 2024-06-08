#pragma once

#include "../module.h"
#include "../module_generator.h"
#include "tofino_context.h"

namespace synapse {
namespace tofino {

class TofinoModule : public Module {
public:
  TofinoModule(ModuleType _type, const std::string &_name,
               const bdd::Node *node)
      : Module(_type, TargetType::Tofino, _name, node) {}

  TofinoModule(ModuleType _type, TargetType _next_type,
               const std::string &_name, const bdd::Node *node)
      : Module(_type, TargetType::Tofino, _next_type, _name, node) {}

  virtual std::unordered_set<DS_ID> get_generated_ds() const { return {}; }
};

class TofinoModuleGenerator : public ModuleGenerator {
protected:
  ModuleType type;
  TargetType target;

public:
  TofinoModuleGenerator(ModuleType _type, const std::string &_name)
      : ModuleGenerator(_type, TargetType::Tofino, _name) {}

protected:
  TofinoContext *get_mutable_tofino_ctx(EP *ep) const {
    Context &ctx = ep->get_mutable_ctx();
    return ctx.get_mutable_target_ctx<TofinoContext>();
  }

  const TofinoContext *get_tofino_ctx(const EP *ep) const {
    const Context &ctx = ep->get_ctx();
    return ctx.get_target_ctx<TofinoContext>();
  }

  TNA &get_mutable_tna(EP *ep) const {
    TofinoContext *ctx = get_mutable_tofino_ctx(ep);
    return ctx->get_mutable_tna();
  }

  const TNA &get_tna(const EP *ep) const {
    const TofinoContext *ctx = get_tofino_ctx(ep);
    return ctx->get_tna();
  }

  symbols_t get_dataplane_state(const EP *ep, const bdd::Node *node) const;

  std::unordered_set<DS *>
  build_registers(const EP *ep, const bdd::Node *node, int num_entries,
                  bits_t index, klee::ref<klee::Expr> value,
                  const std::unordered_set<RegisterAction> &actions,
                  std::unordered_set<DS_ID> &rids,
                  std::unordered_set<DS_ID> &deps) const;

  std::unordered_set<DS *> get_registers(const EP *ep, const bdd::Node *node,
                                         addr_t obj,
                                         std::unordered_set<DS_ID> &rids,
                                         std::unordered_set<DS_ID> &deps) const;

  struct cached_table_data_t {
    addr_t obj;
    klee::ref<klee::Expr> key;
    klee::ref<klee::Expr> read_value;
    klee::ref<klee::Expr> write_value;
    symbol_t map_has_this_key;
    int num_entries;
  };

  DS *build_cached_table(const EP *ep, const bdd::Node *node,
                         const cached_table_data_t &data, int cache_capacity,
                         std::unordered_set<DS_ID> &deps) const;
  DS *get_cached_table(const EP *ep, const cached_table_data_t &data,
                       std::unordered_set<DS_ID> &deps) const;
  DS *get_or_build_cached_table(const EP *ep, const bdd::Node *node,
                                const cached_table_data_t &data,
                                int cache_capacity, bool &already_exists,
                                std::unordered_set<DS_ID> &deps) const;
  bool can_place_cached_table(const EP *ep, const bdd::Call *map_get,
                              map_coalescing_data_t &coalescing_data) const;
  void place_cached_table(EP *ep, const map_coalescing_data_t &coalescing_data,
                          DS *ds, const std::unordered_set<DS_ID> &deps) const;
};

} // namespace tofino
} // namespace synapse
