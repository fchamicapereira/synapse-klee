#pragma once

#include "../data_structures/dchain.h"
#include "../internals/internals.h"

namespace bdd {
namespace emulation {

inline void __dchain_is_index_allocated(const BDD &bdd, const Call *call_node,
                                        pkt_t &pkt, time_ns_t time,
                                        state_t &state, meta_t &meta,
                                        context_t &ctx, const cfg_t &cfg) {
  auto call = call_node->get_call();

  assert(!call.args["chain"].expr.isNull());
  assert(!call.args["index"].expr.isNull());
  assert(!call.ret.isNull());

  auto addr_expr = call.args["chain"].expr;
  auto index_expr = call.args["index"].expr;
  auto is_allocated_expr = call.ret;

  auto addr = kutil::expr_addr_to_obj_addr(addr_expr);
  auto index = kutil::solver_toolbox.value_from_expr(index_expr, ctx);

  auto ds_dchain = state.get(addr);
  auto dchain = Dchain::cast(ds_dchain);

  auto is_allocated = dchain->is_index_allocated(index);
  concretize(ctx, is_allocated_expr, is_allocated);
}

inline std::pair<std::string, operation_ptr> dchain_is_index_allocated() {
  return {"dchain_is_index_allocated", __dchain_is_index_allocated};
}

} // namespace emulation
} // namespace bdd