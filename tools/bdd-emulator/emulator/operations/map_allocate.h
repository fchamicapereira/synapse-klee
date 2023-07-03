#pragma once

#include "../data_structures/map.h"
#include "../internals/internals.h"

namespace BDD {
namespace emulation {

inline void __map_allocate(const BDD& bdd, const Call *call_node, pkt_t &pkt, time_ns_t time,
                           state_t &state, meta_t &meta, context_t &ctx,
                           const cfg_t &cfg) {
  auto call = call_node->get_call();

  assert(!call.args[symbex::FN_MAP_ARG_CAPACITY].expr.isNull());
  assert(!call.args[symbex::FN_MAP_ARG_MAP_OUT].out.isNull());

  auto addr_expr = call.args[symbex::FN_MAP_ARG_MAP_OUT].out;
  auto capacity_expr = call.args[symbex::FN_MAP_ARG_CAPACITY].expr;

  auto addr = kutil::expr_addr_to_obj_addr(addr_expr);
  auto capacity = kutil::solver_toolbox.value_from_expr(capacity_expr);

  auto map = DataStructureRef(new Map(addr, capacity));
  state.add(map);
}

inline std::pair<std::string, operation_ptr> map_allocate() {
  return {symbex::FN_MAP_ALLOCATE, __map_allocate};
}

} // namespace emulation
} // namespace BDD