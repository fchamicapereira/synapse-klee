#pragma once

#include "../data_structures/dchain.h"
#include "../internals/internals.h"

namespace BDD {
namespace emulation {

inline void __dchain_allocate_new_index(const BDD &bdd, const Call *call_node,
                                        pkt_t &pkt, time_ns_t time,
                                        state_t &state, meta_t &meta,
                                        context_t &ctx, const cfg_t &cfg) {
  auto call = call_node->get_call();

  assert(!call.args[symbex::FN_DCHAIN_ARG_CHAIN].expr.isNull());
  assert(!call.args[symbex::FN_DCHAIN_ARG_TIME].expr.isNull());
  assert(!call.args[symbex::FN_DCHAIN_ARG_OUT].out.isNull());

  auto addr_expr = call.args[symbex::FN_DCHAIN_ARG_CHAIN].expr;
  auto index_out_expr = call.args[symbex::FN_DCHAIN_ARG_OUT].out;

  auto addr = kutil::expr_addr_to_obj_addr(addr_expr);

  auto generated_symbols = call_node->get_local_generated_symbols();
  auto out_of_space_symbol =
      get_symbol(generated_symbols, symbex::DCHAIN_OUT_OF_SPACE);
  auto out_of_space_expr = bdd.get_symbol(out_of_space_symbol.label);

  auto ds_dchain = state.get(addr);
  auto dchain = Dchain::cast(ds_dchain);

  uint32_t index_out;
  auto out_of_space = !dchain->allocate_new_index(index_out, time);

  concretize(ctx, out_of_space_expr, out_of_space);

  if (!out_of_space) {
    concretize(ctx, index_out_expr, index_out);
  }

  meta.dchain_allocations++;
}

inline std::pair<std::string, operation_ptr> dchain_allocate_new_index() {
  return {symbex::FN_DCHAIN_ALLOCATE_NEW_INDEX, __dchain_allocate_new_index};
}

} // namespace emulation
} // namespace BDD