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

static FCFSCachedTable *build_fcfs_cached_table(const EP *ep,
                                                const bdd::Node *node,
                                                klee::ref<klee::Expr> key,
                                                int num_entries,
                                                int cache_capacity) {
  const Context &ctx = ep->get_ctx();
  const TofinoContext *tofino_ctx = ctx.get_target_ctx<TofinoContext>();
  const TNA &tna = tofino_ctx->get_tna();
  const TNAProperties &properties = tna.get_properties();

  DS_ID id = "cached_table_" + std::to_string(node->get_id()) + "_" +
             std::to_string(cache_capacity);

  std::vector<klee::ref<klee::Expr>> keys =
      Register::partition_value(properties, key);
  std::vector<bits_t> keys_sizes;
  for (klee::ref<klee::Expr> key : keys) {
    keys_sizes.push_back(key->getWidth());
  }

  FCFSCachedTable *cached_table = new FCFSCachedTable(
      properties, id, cache_capacity, num_entries, keys_sizes);

  std::unordered_set<DS_ID> deps = tofino_ctx->get_stateful_deps(ep, node);
  if (!tofino_ctx->check_placement(ep, cached_table, deps)) {
    delete cached_table;
    cached_table = nullptr;
  }

  return cached_table;
}

static FCFSCachedTable *
reuse_fcfs_cached_table(const EP *ep, const bdd::Node *node, addr_t obj) {
  const Context &ctx = ep->get_ctx();
  const TofinoContext *tofino_ctx = ctx.get_target_ctx<TofinoContext>();

  if (!tofino_ctx->has_ds(obj)) {
    return nullptr;
  }

  const std::vector<DS *> &ds = tofino_ctx->get_ds(obj);

  assert(ds.size() == 1);
  assert(ds[0]->type == DSType::FCFS_CACHED_TABLE);

  FCFSCachedTable *cached_table = static_cast<FCFSCachedTable *>(ds[0]);
  cached_table->add_table();

  std::unordered_set<DS_ID> deps = tofino_ctx->get_stateful_deps(ep, node);
  if (!tofino_ctx->check_placement(ep, cached_table, deps)) {
    cached_table = nullptr;
  }

  return cached_table;
}

FCFSCachedTable *TofinoModuleGenerator::build_or_reuse_fcfs_cached_table(
    const EP *ep, const bdd::Node *node, addr_t obj, klee::ref<klee::Expr> key,
    int num_entries, int cache_capacity) const {
  FCFSCachedTable *cached_table = nullptr;

  const Context &ctx = ep->get_ctx();
  bool already_placed =
      ctx.check_placement(obj, PlacementDecision::Tofino_FCFSCachedTable);

  if (already_placed) {
    cached_table = reuse_fcfs_cached_table(ep, node, obj);

    if (!cached_table || cached_table->cache_capacity != cache_capacity) {
      return nullptr;
    }
  } else {
    cached_table =
        build_fcfs_cached_table(ep, node, key, num_entries, cache_capacity);
  }

  return cached_table;
}

FCFSCachedTable *TofinoModuleGenerator::get_fcfs_cached_table(
    const EP *ep, const bdd::Node *node, addr_t obj) const {
  const Context &ctx = ep->get_ctx();
  const TofinoContext *tofino_ctx = ctx.get_target_ctx<TofinoContext>();

  if (!tofino_ctx->has_ds(obj)) {
    return nullptr;
  }

  const std::vector<DS *> &ds = tofino_ctx->get_ds(obj);

  assert(ds.size() == 1);
  assert(ds[0]->type == DSType::FCFS_CACHED_TABLE);

  FCFSCachedTable *cached_table = static_cast<FCFSCachedTable *>(ds[0]);

  std::unordered_set<DS_ID> deps = tofino_ctx->get_stateful_deps(ep, node);
  if (!tofino_ctx->check_placement(ep, cached_table, deps)) {
    cached_table = nullptr;
  }

  return cached_table;
}

bool TofinoModuleGenerator::can_get_or_build_fcfs_cached_table(
    const EP *ep, const bdd::Node *node, addr_t obj, klee::ref<klee::Expr> key,
    int num_entries, int cache_capacity) const {
  FCFSCachedTable *cached_table = nullptr;

  const Context &ctx = ep->get_ctx();
  bool already_placed =
      ctx.check_placement(obj, PlacementDecision::Tofino_FCFSCachedTable);

  if (already_placed) {
    cached_table = get_fcfs_cached_table(ep, node, obj);

    if (!cached_table || cached_table->cache_capacity != cache_capacity) {
      return false;
    }

    return true;
  }

  cached_table =
      build_fcfs_cached_table(ep, node, key, num_entries, cache_capacity);

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

bool TofinoModuleGenerator::can_place_fcfs_cached_table(
    const EP *ep, const map_coalescing_objs_t &map_objs) const {
  const Context &ctx = ep->get_ctx();

  return ctx.can_place(map_objs.map,
                       PlacementDecision::Tofino_FCFSCachedTable) &&
         ctx.can_place(map_objs.dchain,
                       PlacementDecision::Tofino_FCFSCachedTable) &&
         ctx.can_place(map_objs.vector_key,
                       PlacementDecision::Tofino_FCFSCachedTable);
}

void TofinoModuleGenerator::place_fcfs_cached_table(
    EP *ep, const bdd::Node *node, const map_coalescing_objs_t &map_objs,
    FCFSCachedTable *ds) const {
  assert(ds->type == DSType::FCFS_CACHED_TABLE);
  FCFSCachedTable *cached_table = static_cast<FCFSCachedTable *>(ds);

  place(ep, map_objs.map, PlacementDecision::Tofino_FCFSCachedTable);
  place(ep, map_objs.dchain, PlacementDecision::Tofino_FCFSCachedTable);
  place(ep, map_objs.vector_key, PlacementDecision::Tofino_FCFSCachedTable);

  TofinoContext *tofino_ctx = get_mutable_tofino_ctx(ep);
  std::unordered_set<DS_ID> deps = tofino_ctx->get_stateful_deps(ep, node);
  tofino_ctx->place(ep, map_objs.map, cached_table, deps);
  tofino_ctx->place(ep, map_objs.dchain, cached_table, deps);
  tofino_ctx->place(ep, map_objs.vector_key, cached_table, deps);

  Log::dbg() << "-> ~~~ NEW PLACEMENT ~~~ <-\n";
  ds->log_debug();
  tofino_ctx->get_tna().log_debug_placement();
}

std::unordered_set<int>
TofinoModuleGenerator::enumerate_fcfs_cache_table_capacities(
    int num_entries) const {
  std::unordered_set<int> capacities;

  int cache_capacity = 8;
  while (1) {
    capacities.insert(cache_capacity);
    cache_capacity *= 2;

    if (cache_capacity > num_entries) {
      break;
    }
  }

  return capacities;
}

} // namespace tofino
} // namespace synapse