#include "tofino_module.h"

namespace synapse {
namespace tofino {

std::unordered_set<DS *> TofinoModuleGenerator::build_vector_registers(
    const EP *ep, const bdd::Node *node, const vector_register_data_t &data,
    std::unordered_set<DS_ID> &rids, std::unordered_set<DS_ID> &deps) const {
  std::unordered_set<DS *> regs;

  const TNA &tna = get_tna(ep);
  const TNAProperties &properties = tna.get_properties();

  std::vector<klee::ref<klee::Expr>> partitions =
      Register::partition_value(properties, data.value);

  for (klee::ref<klee::Expr> partition : partitions) {
    DS_ID rid = "vector_" + std::to_string(node->get_id()) + "_" +
                std::to_string(rids.size());
    bits_t partition_size = partition->getWidth();
    Register *reg =
        new Register(properties, rid, data.num_entries, data.index->getWidth(),
                     partition_size, data.actions);
    regs.insert(reg);
    rids.insert(rid);
  }

  const TofinoContext *tofino_ctx = get_tofino_ctx(ep);
  deps = tofino_ctx->get_stateful_deps(ep, node);

  if (!tofino_ctx->check_many_placements(ep, {regs}, deps)) {
    for (DS *reg : regs) {
      delete reg;
    }
    regs.clear();
  }

  return regs;
}

std::unordered_set<DS *> TofinoModuleGenerator::get_vector_registers(
    const EP *ep, const bdd::Node *node, const vector_register_data_t &data,
    std::unordered_set<DS_ID> &rids, std::unordered_set<DS_ID> &deps) const {
  std::unordered_set<DS *> regs;

  const TofinoContext *tofino_ctx = get_tofino_ctx(ep);

  if (!tofino_ctx->has_ds(data.obj)) {
    return regs;
  }

  const std::vector<DS *> &ds = tofino_ctx->get_ds(data.obj);
  assert(ds.size());

  for (DS *reg : ds) {
    assert(reg->type == DSType::REGISTER);
    regs.insert(reg);
    rids.insert(reg->id);
  }

  deps = tofino_ctx->get_stateful_deps(ep, node);

  if (!tofino_ctx->check_many_placements(ep, {regs}, deps)) {
    regs.clear();
  }

  return regs;
}

std::unordered_set<DS *> TofinoModuleGenerator::get_or_build_vector_registers(
    const EP *ep, const bdd::Node *node, const vector_register_data_t &data,
    bool &already_exists, std::unordered_set<DS_ID> &rids,
    std::unordered_set<DS_ID> &deps) const {
  std::unordered_set<DS *> regs;

  const Context &ctx = ep->get_ctx();
  bool regs_already_placed =
      ctx.check_placement(data.obj, PlacementDecision::Tofino_VectorRegister);

  if (regs_already_placed) {
    regs = get_vector_registers(ep, node, data, rids, deps);
    already_exists = true;
  } else {
    regs = build_vector_registers(ep, node, data, rids, deps);
    already_exists = false;
  }

  return regs;
}

bool TofinoModuleGenerator::can_get_or_build_vector_registers(
    const EP *ep, const bdd::Node *node,
    const vector_register_data_t &data) const {
  std::unordered_set<DS_ID> rids;
  std::unordered_set<DS_ID> deps;

  const Context &ctx = ep->get_ctx();
  bool regs_already_placed =
      ctx.check_placement(data.obj, PlacementDecision::Tofino_VectorRegister);

  if (regs_already_placed) {
    std::unordered_set<DS *> regs =
        get_vector_registers(ep, node, data, rids, deps);
    return !regs.empty();
  }

  std::unordered_set<DS *> regs =
      build_vector_registers(ep, node, data, rids, deps);
  bool success = !regs.empty();

  for (DS *reg : regs) {
    delete reg;
  }

  return success;
}

void TofinoModuleGenerator::place_vector_registers(
    EP *ep, const vector_register_data_t &data,
    const std::unordered_set<DS *> &regs,
    const std::unordered_set<DS_ID> &deps) const {
  TofinoContext *tofino_ctx = get_mutable_tofino_ctx(ep);
  place(ep, data.obj, PlacementDecision::Tofino_VectorRegister);
  tofino_ctx->place_many(ep, data.obj, {regs}, deps);

  Log::dbg() << "-> ~~~ NEW PLACEMENT ~~~ <-\n";
  for (DS *reg : regs) {
    reg->log_debug();
  }
  tofino_ctx->get_tna().log_debug_placement();
}

TTLCachedTable *TofinoModuleGenerator::build_cached_table(
    const EP *ep, const bdd::Node *node, const cached_table_data_t &data,
    int cache_capacity, std::unordered_set<DS_ID> &deps) const {
  const TNA &tna = get_tna(ep);
  const TNAProperties &properties = tna.get_properties();

  DS_ID id = "cached_table_" + std::to_string(node->get_id()) + "_" +
             std::to_string(cache_capacity);

  std::vector<klee::ref<klee::Expr>> keys =
      Register::partition_value(properties, data.key);
  std::vector<bits_t> keys_sizes;
  for (klee::ref<klee::Expr> key : keys) {
    keys_sizes.push_back(key->getWidth());
  }

  TTLCachedTable *cached_table = new TTLCachedTable(
      properties, id, cache_capacity, data.num_entries, keys_sizes);

  const TofinoContext *tofino_ctx = get_tofino_ctx(ep);
  deps = tofino_ctx->get_stateful_deps(ep, node);
  if (!tofino_ctx->check_placement(ep, cached_table, deps)) {
    delete cached_table;
    cached_table = nullptr;
  }

  return cached_table;
}

TTLCachedTable *
TofinoModuleGenerator::get_cached_table(const EP *ep, const bdd::Node *node,
                                        const cached_table_data_t &data,
                                        std::unordered_set<DS_ID> &deps) const {
  const TofinoContext *tofino_ctx = get_tofino_ctx(ep);

  if (!tofino_ctx->has_ds(data.obj)) {
    return nullptr;
  }

  const std::vector<DS *> &ds = tofino_ctx->get_ds(data.obj);

  assert(ds.size() == 1);
  assert(ds[0]->type == DSType::CACHED_TABLE);

  TTLCachedTable *cached_table = static_cast<TTLCachedTable *>(ds[0]);

  deps = tofino_ctx->get_stateful_deps(ep, node);
  if (!tofino_ctx->check_placement(ep, cached_table, deps)) {
    cached_table = nullptr;
  }

  return cached_table;
}

TTLCachedTable *TofinoModuleGenerator::get_or_build_cached_table(
    const EP *ep, const bdd::Node *node, const cached_table_data_t &data,
    int cache_capacity, bool &already_exists,
    std::unordered_set<DS_ID> &deps) const {
  TTLCachedTable *cached_table = nullptr;

  const Context &ctx = ep->get_ctx();
  bool already_placed =
      ctx.check_placement(data.obj, PlacementDecision::Tofino_TTLCachedTable);

  if (already_placed) {
    cached_table = get_cached_table(ep, node, data, deps);
    already_exists = true;

    if (!cached_table || cached_table->cache_capacity != cache_capacity) {
      return nullptr;
    }
  } else {
    cached_table = build_cached_table(ep, node, data, cache_capacity, deps);
    already_exists = false;
  }

  return cached_table;
}

bool TofinoModuleGenerator::can_get_or_build_cached_table(
    const EP *ep, const bdd::Node *node, const cached_table_data_t &data,
    int cache_capacity) const {
  std::unordered_set<DS_ID> deps;

  const Context &ctx = ep->get_ctx();
  bool already_placed =
      ctx.check_placement(data.obj, PlacementDecision::Tofino_TTLCachedTable);

  if (already_placed) {
    TTLCachedTable *cached_table = get_cached_table(ep, node, data, deps);

    if (!cached_table || (cached_table->cache_capacity != cache_capacity)) {
      return false;
    }

    return true;
  }

  TTLCachedTable *cached_table =
      build_cached_table(ep, node, data, cache_capacity, deps);

  if (!cached_table) {
    return false;
  }

  delete cached_table;
  return true;
}

symbols_t
TofinoModuleGenerator::get_dataplane_state(const EP *ep,
                                           const bdd::Node *node) const {
  const bdd::nodes_t &roots = ep->get_target_roots(TargetType::Tofino);
  return get_prev_symbols(node, roots);
}

bool TofinoModuleGenerator::can_place_cached_table(
    const EP *ep, const map_coalescing_data_t &coalescing_data) const {
  addr_t map_obj = coalescing_data.map;
  addr_t dchain_obj = coalescing_data.dchain;
  addr_t vector_key_obj = coalescing_data.vector_key;

  const Context &ctx = ep->get_ctx();

  return ctx.can_place(map_obj, PlacementDecision::Tofino_TTLCachedTable) &&
         ctx.can_place(dchain_obj, PlacementDecision::Tofino_TTLCachedTable) &&
         ctx.can_place(vector_key_obj,
                       PlacementDecision::Tofino_TTLCachedTable);
}

void TofinoModuleGenerator::place_cached_table(
    EP *ep, const map_coalescing_data_t &coalescing_data, TTLCachedTable *ds,
    const std::unordered_set<DS_ID> &deps) const {
  assert(ds->type == DSType::CACHED_TABLE);
  TTLCachedTable *cached_table = static_cast<TTLCachedTable *>(ds);

  addr_t map_obj = coalescing_data.map;
  place(ep, map_obj, PlacementDecision::Tofino_TTLCachedTable);

  TofinoContext *tofino_ctx = get_mutable_tofino_ctx(ep);
  tofino_ctx->place(ep, map_obj, cached_table, deps);

  Log::dbg() << "-> ~~~ NEW PLACEMENT ~~~ <-\n";
  ds->log_debug();
  tofino_ctx->get_tna().log_debug_placement();
}

std::unordered_set<int>
TofinoModuleGenerator::enumerate_cache_table_capacities(int num_entries) const {
  std::unordered_set<int> capacities;

  int cache_capacity = 1024;
  while (cache_capacity <= num_entries) {
    capacities.insert(cache_capacity);
    cache_capacity *= 2;
  }

  return capacities;
}

} // namespace tofino
} // namespace synapse