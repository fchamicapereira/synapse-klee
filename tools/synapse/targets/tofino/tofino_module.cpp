#include "tofino_module.h"

namespace synapse {
namespace tofino {

std::unordered_set<DS *> TofinoModuleGenerator::build_registers(
    const EP *ep, const bdd::Node *node, int num_entries, bits_t index,
    klee::ref<klee::Expr> value,
    const std::unordered_set<RegisterAction> &actions,
    std::unordered_set<DS_ID> &rids, std::unordered_set<DS_ID> &deps) const {
  std::unordered_set<DS *> regs;

  const TNA &tna = get_tna(ep);
  const TNAConstraints &tna_constr = tna.get_constraints();

  std::vector<klee::ref<klee::Expr>> partitions =
      Register::partition_value(tna_constr, value);

  for (klee::ref<klee::Expr> partition : partitions) {
    DS_ID rid = "vector_" + std::to_string(node->get_id()) + "_" +
                std::to_string(rids.size());
    bits_t partition_size = partition->getWidth();
    Register *reg = new Register(tna_constr, rid, num_entries, index,
                                 partition_size, actions);
    regs.insert(reg);
    rids.insert(rid);
  }

  const TofinoContext *tofino_ctx = get_tofino_ctx(ep);
  deps = tofino_ctx->get_stateful_deps(ep);

  if (!tofino_ctx->check_many_placements(ep, {regs}, deps)) {
    for (DS *reg : regs) {
      delete reg;
    }
    regs.clear();
  }

  return regs;
}

std::unordered_set<DS *> TofinoModuleGenerator::get_registers(
    const EP *ep, const bdd::Node *node, addr_t obj,
    std::unordered_set<DS_ID> &rids, std::unordered_set<DS_ID> &deps) const {
  std::unordered_set<DS *> regs;

  const TofinoContext *tofino_ctx = get_tofino_ctx(ep);
  const std::vector<DS *> &ds = tofino_ctx->get_ds(obj);
  assert(ds.size());

  for (DS *reg : ds) {
    assert(reg->type == DSType::REGISTER);
    regs.insert(reg);
    rids.insert(reg->id);
  }

  deps = tofino_ctx->get_stateful_deps(ep);
  if (!tofino_ctx->check_many_placements(ep, {regs}, deps)) {
    regs.clear();
  }

  return regs;
}

CachedTable *TofinoModuleGenerator::build_cached_table(
    const EP *ep, const bdd::Node *node, const cached_table_data_t &data,
    int cache_capacity, std::unordered_set<DS_ID> &deps) const {
  const TNA &tna = get_tna(ep);
  const TNAConstraints &tna_constr = tna.get_constraints();

  DS_ID id = "cached_table_" + std::to_string(node->get_id()) + "_" +
             std::to_string(cache_capacity);

  std::vector<klee::ref<klee::Expr>> keys =
      Register::partition_value(tna_constr, data.key);
  std::vector<bits_t> keys_sizes;
  for (klee::ref<klee::Expr> key : keys) {
    keys_sizes.push_back(key->getWidth());
  }

  std::vector<klee::ref<klee::Expr>> values =
      Register::partition_value(tna_constr, data.read_value);
  assert(values.size() == 1);
  bits_t value_size = data.read_value->getWidth();

  CachedTable *cached_table = new CachedTable(
      tna_constr, id, cache_capacity, data.num_entries, keys_sizes, value_size);

  const TofinoContext *tofino_ctx = get_tofino_ctx(ep);
  deps = tofino_ctx->get_stateful_deps(ep);
  if (!tofino_ctx->check_placement(ep, cached_table, deps)) {
    delete cached_table;
    cached_table = nullptr;
  }

  return cached_table;
}

CachedTable *
TofinoModuleGenerator::get_cached_table(const EP *ep,
                                        const cached_table_data_t &data,
                                        std::unordered_set<DS_ID> &deps) const {
  const TofinoContext *tofino_ctx = get_tofino_ctx(ep);
  const std::vector<DS *> &ds = tofino_ctx->get_ds(data.obj);

  assert(ds.size() == 1);
  assert(ds[0]->type == DSType::CACHED_TABLE);

  CachedTable *cached_table = static_cast<CachedTable *>(ds[0]);

  deps = tofino_ctx->get_stateful_deps(ep);
  if (!tofino_ctx->check_placement(ep, cached_table, deps)) {
    cached_table = nullptr;
  }

  return cached_table;
}

CachedTable *TofinoModuleGenerator::get_or_build_cached_table(
    const EP *ep, const bdd::Node *node, const cached_table_data_t &data,
    int cache_capacity, bool &already_exists,
    std::unordered_set<DS_ID> &deps) const {
  CachedTable *cached_table = nullptr;

  const Context &ctx = ep->get_ctx();
  bool already_placed =
      ctx.check_placement(data.obj, PlacementDecision::Tofino_CachedTable);

  if (already_placed) {
    cached_table = get_cached_table(ep, data, deps);
    already_exists = true;
  } else {
    cached_table = build_cached_table(ep, node, data, cache_capacity, deps);
    already_exists = false;
  }

  return cached_table;
}

symbols_t
TofinoModuleGenerator::get_dataplane_state(const EP *ep,
                                           const bdd::Node *node) const {
  const bdd::nodes_t &roots = ep->get_target_roots(TargetType::Tofino);
  return get_prev_symbols(node, roots);
}

bool TofinoModuleGenerator::can_place_cached_table(
    const EP *ep, const bdd::Call *map_get,
    map_coalescing_data_t &coalescing_data) const {
  const call_t &call = map_get->get_call();
  assert(call.function_name == "map_get");

  klee::ref<klee::Expr> obj_expr = call.args.at("map").expr;

  addr_t map_obj = kutil::expr_addr_to_obj_addr(obj_expr);
  addr_t dchain_obj = coalescing_data.dchain;
  addr_t vector_key_obj = coalescing_data.vector_key;

  const Context &ctx = ep->get_ctx();
  std::optional<map_coalescing_data_t> data = ctx.get_coalescing_data(map_obj);

  if (!data.has_value()) {
    return false;
  }

  coalescing_data = data.value();

  return ctx.can_place(map_obj, PlacementDecision::Tofino_CachedTable) &&
         ctx.can_place(dchain_obj, PlacementDecision::Tofino_CachedTable) &&
         ctx.can_place(vector_key_obj, PlacementDecision::Tofino_CachedTable);
}

void TofinoModuleGenerator::place_cached_table(
    EP *ep, const map_coalescing_data_t &coalescing_data, CachedTable *ds,
    const std::unordered_set<DS_ID> &deps) const {
  assert(ds->type == DSType::CACHED_TABLE);
  CachedTable *cached_table = static_cast<CachedTable *>(ds);

  addr_t map_obj = coalescing_data.map;
  place(ep, map_obj, PlacementDecision::Tofino_CachedTable);

  TofinoContext *tofino_ctx = get_mutable_tofino_ctx(ep);
  tofino_ctx->place(ep, map_obj, cached_table, deps);
}

} // namespace tofino
} // namespace synapse