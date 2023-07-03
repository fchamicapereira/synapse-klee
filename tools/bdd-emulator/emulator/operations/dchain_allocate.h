#pragma once

#include "../data_structures/dchain.h"
#include "../internals/internals.h"

namespace BDD {
namespace emulation {

inline void __dchain_allocate(const BDD& bdd, const Call *call_node, pkt_t &pkt, time_ns_t time,
                              state_t &state, meta_t &meta, context_t &ctx,
                              const cfg_t &cfg) {
  auto call = call_node->get_call();

  assert(!call.args[symbex::FN_DCHAIN_ALLOCATE_ARG_CHAIN_OUT].out.isNull());
  assert(!call.args[symbex::FN_DCHAIN_ALLOCATE_ARG_INDEX_RANGE].expr.isNull());

  auto addr_expr = call.args[symbex::FN_DCHAIN_ALLOCATE_ARG_CHAIN_OUT].out;
  auto index_range_expr =
      call.args[symbex::FN_DCHAIN_ALLOCATE_ARG_INDEX_RANGE].expr;

  auto addr = kutil::expr_addr_to_obj_addr(addr_expr);
  auto index_range = kutil::solver_toolbox.value_from_expr(index_range_expr);

  auto dchain = DataStructureRef(new Dchain(addr, index_range));
  state.add(dchain);
}

inline std::pair<std::string, operation_ptr> dchain_allocate() {
  return {symbex::FN_DCHAIN_ALLOCATE, __dchain_allocate};
}

} // namespace emulation
} // namespace BDD