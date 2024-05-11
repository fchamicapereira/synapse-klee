#pragma once

#include "../data_structures/map.h"
#include "../internals/internals.h"

namespace bdd {
namespace emulation {

inline void __map_allocate(const BDD &bdd, const Call *call_node, pkt_t &pkt,
                           time_ns_t time, state_t &state, meta_t &meta,
                           context_t &ctx, const cfg_t &cfg) {
  auto call = call_node->get_call();

  assert(!call.args["capacity"].expr.isNull());
  assert(!call.args["map_out"].out.isNull());

  auto addr_expr = call.args["map_out"].out;
  auto capacity_expr = call.args["capacity"].expr;

  auto addr = kutil::expr_addr_to_obj_addr(addr_expr);
  auto capacity = kutil::solver_toolbox.value_from_expr(capacity_expr);

  auto map = DataStructureRef(new Map(addr, capacity));
  state.add(map);
}

inline std::pair<std::string, operation_ptr> map_allocate() {
  return {"map_allocate", __map_allocate};
}

} // namespace emulation
} // namespace bdd