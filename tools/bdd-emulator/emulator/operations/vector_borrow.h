#pragma once

#include "../data_structures/vector.h"
#include "../internals/internals.h"

namespace BDD {
namespace emulation {

inline void __vector_borrow(const BDD& bdd, const Call *call_node, pkt_t &pkt, time_ns_t time,
                            state_t &state, meta_t &meta, context_t &ctx,
                            const cfg_t &cfg) {
  auto call = call_node->get_call();

  assert(!call.args[symbex::FN_VECTOR_ARG_VECTOR].expr.isNull());
  assert(!call.args[symbex::FN_VECTOR_ARG_INDEX].expr.isNull());
  assert(!call.extra_vars[symbex::FN_VECTOR_EXTRA].second.isNull());

  auto addr_expr = call.args[symbex::FN_VECTOR_ARG_VECTOR].expr;
  auto index_expr = call.args[symbex::FN_VECTOR_ARG_INDEX].expr;
  auto value_expr = call.extra_vars[symbex::FN_VECTOR_EXTRA].second;

  auto addr = kutil::expr_addr_to_obj_addr(addr_expr);
  auto index = kutil::solver_toolbox.value_from_expr(index_expr, ctx);

  auto ds_vector = state.get(addr);
  auto vector = Vector::cast(ds_vector);

  auto value = vector->get(index);
  concretize(ctx, value_expr, value.values);
}

inline std::pair<std::string, operation_ptr> vector_borrow() {
  return {symbex::FN_VECTOR_BORROW, __vector_borrow};
}

} // namespace emulation
} // namespace BDD