#pragma once

#include "../data_structures/vector.h"
#include "../internals/internals.h"

namespace bdd {
namespace emulation {

inline void __vector_return(const BDD &bdd, const Call *call_node, pkt_t &pkt,
                            time_ns_t time, state_t &state, meta_t &meta,
                            context_t &ctx, const cfg_t &cfg) {
  auto call = call_node->get_call();

  assert(!call.args["vector"].expr.isNull());
  assert(!call.args["index"].expr.isNull());
  assert(!call.args["value"].in.isNull());

  auto addr_expr = call.args["vector"].expr;
  auto index_expr = call.args["index"].expr;
  auto value_expr = call.args["value"].in;

  auto addr = kutil::expr_addr_to_obj_addr(addr_expr);
  auto index = kutil::solver_toolbox.value_from_expr(index_expr, ctx);
  auto value = bytes_from_expr(value_expr, ctx);

  auto ds_vector = state.get(addr);
  auto vector = Vector::cast(ds_vector);

  vector->put(index, value);
}

inline std::pair<std::string, operation_ptr> vector_return() {
  return {"vector_return", __vector_return};
}

} // namespace emulation
} // namespace bdd