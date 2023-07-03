#pragma once

#include "../data_structures/map.h"
#include "../data_structures/vector.h"
#include "../internals/internals.h"

namespace BDD {
namespace emulation {

inline void __expire_items_single_map_iteratively(
    const BDD &bdd, const Call *call_node, pkt_t &pkt, time_ns_t time,
    state_t &state, meta_t &meta, context_t &ctx, const cfg_t &cfg) {
  auto call = call_node->get_call();

  assert(!call.args[symbex::FN_EXPIRE_MAP_ARG_MAP].expr.isNull());
  assert(!call.args[symbex::FN_EXPIRE_MAP_ARG_VECTOR].expr.isNull());
  assert(!call.args[symbex::EXPIRE_MAP_ITERATIVELY_START].expr.isNull());
  assert(!call.args[symbex::EXPIRE_MAP_ITERATIVELY_N_ELEMS].expr.isNull());

  auto map_addr_expr = call.args[symbex::EXPIRE_MAP_ITERATIVELY_MAP].expr;
  auto vector_addr_expr = call.args[symbex::EXPIRE_MAP_ITERATIVELY_VECTOR].expr;
  auto start_expr = call.args[symbex::EXPIRE_MAP_ITERATIVELY_START].expr;
  auto n_elems_expr = call.args[symbex::EXPIRE_MAP_ITERATIVELY_N_ELEMS].expr;

  auto map_addr = kutil::expr_addr_to_obj_addr(map_addr_expr);
  auto vector_addr = kutil::expr_addr_to_obj_addr(vector_addr_expr);

  auto start = kutil::solver_toolbox.value_from_expr(start_expr, ctx);
  auto n_elems = kutil::solver_toolbox.value_from_expr(n_elems_expr, ctx);

  auto ds_map = state.get(map_addr);
  auto ds_vector = state.get(vector_addr);

  auto map = Map::cast(ds_map);
  auto vector = Vector::cast(ds_vector);

  for (auto i = start; i < n_elems; i++) {
    auto key = vector->get(i);
    map->erase(key);
  }
}

inline std::pair<std::string, operation_ptr>
expire_items_single_map_iteratively() {
  return {symbex::FN_EXPIRE_MAP_ITERATIVELY,
          __expire_items_single_map_iteratively};
}

} // namespace emulation
} // namespace BDD