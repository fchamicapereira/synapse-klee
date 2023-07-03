#pragma once

#include "../data_structures/vector.h"
#include "../internals/internals.h"

namespace BDD {
namespace emulation {

inline void __vector_allocate(const BDD& bdd, const Call *call_node, pkt_t &pkt, time_ns_t time,
                              state_t &state, meta_t &meta, context_t &ctx,
                              const cfg_t &cfg) {
  auto call = call_node->get_call();

  assert(!call.args[symbex::FN_VECTOR_ARG_CAPACITY].expr.isNull());
  assert(!call.args[symbex::FN_VECTOR_ARG_ELEM_SIZE].expr.isNull());
  assert(!call.args[symbex::FN_VECTOR_ARG_VECTOR_OUT].out.isNull());

  auto addr_expr = call.args[symbex::FN_VECTOR_ARG_VECTOR_OUT].out;
  auto elem_size_expr = call.args[symbex::FN_VECTOR_ARG_ELEM_SIZE].expr;
  auto capacity_expr = call.args[symbex::FN_VECTOR_ARG_CAPACITY].expr;

  auto addr = kutil::expr_addr_to_obj_addr(addr_expr);
  auto elem_size = kutil::solver_toolbox.value_from_expr(elem_size_expr);
  auto capacity = kutil::solver_toolbox.value_from_expr(capacity_expr);

  auto vector = DataStructureRef(new Vector(addr, elem_size, capacity));
  state.add(vector);
}

inline std::pair<std::string, operation_ptr> vector_allocate() {
  return {symbex::FN_VECTOR_ALLOCATE, __vector_allocate};
}

} // namespace emulation
} // namespace BDD