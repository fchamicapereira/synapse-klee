#pragma once

#include "../data_structures/dchain.h"
#include "../internals/internals.h"

namespace BDD {
namespace emulation {

inline void __dchain_rejuvenate_index(const BDD& bdd, const Call *call_node, pkt_t &pkt,
                                      time_ns_t time, state_t &state,
                                      meta_t &meta, context_t &ctx,
                                      const cfg_t &cfg) {
  auto call = call_node->get_call();

  assert(!call.args[symbex::FN_DCHAIN_ARG_CHAIN].expr.isNull());
  assert(!call.args[symbex::FN_DCHAIN_ARG_INDEX].expr.isNull());
  assert(!call.args[symbex::FN_DCHAIN_ARG_TIME].expr.isNull());

  auto addr_expr = call.args[symbex::FN_DCHAIN_ARG_CHAIN].expr;
  auto index_expr = call.args[symbex::FN_DCHAIN_ARG_INDEX].expr;

  auto addr = kutil::expr_addr_to_obj_addr(addr_expr);
  auto index = kutil::solver_toolbox.value_from_expr(index_expr, ctx);

  auto ds_dchain = state.get(addr);
  auto dchain = Dchain::cast(ds_dchain);

  dchain->rejuvenate_index(index, time);
}

inline std::pair<std::string, operation_ptr> dchain_rejuvenate_index() {
  return {symbex::FN_DCHAIN_REJUVENATE, __dchain_rejuvenate_index};
}

} // namespace emulation
} // namespace BDD