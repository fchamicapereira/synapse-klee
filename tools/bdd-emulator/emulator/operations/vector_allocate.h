#pragma once

#include "../data_structures/vector.h"
#include "../internals/internals.h"

namespace bdd {
namespace emulation {

inline void __vector_allocate(const BDD &bdd, const Call *call_node, pkt_t &pkt,
                              time_ns_t time, state_t &state, meta_t &meta,
                              context_t &ctx, const cfg_t &cfg) {
  auto call = call_node->get_call();

  assert(!call.args["capacity"].expr.isNull());
  assert(!call.args["elem_size"].expr.isNull());
  assert(!call.args["vector_out"].out.isNull());

  auto addr_expr = call.args["vector_out"].out;
  auto elem_size_expr = call.args["elem_size"].expr;
  auto capacity_expr = call.args["capacity"].expr;

  auto addr = kutil::expr_addr_to_obj_addr(addr_expr);
  auto elem_size = kutil::solver_toolbox.value_from_expr(elem_size_expr);
  auto capacity = kutil::solver_toolbox.value_from_expr(capacity_expr);

  auto vector = DataStructureRef(new Vector(addr, elem_size, capacity));
  state.add(vector);
}

inline std::pair<std::string, operation_ptr> vector_allocate() {
  return {"vector_allocate", __vector_allocate};
}

} // namespace emulation
} // namespace bdd