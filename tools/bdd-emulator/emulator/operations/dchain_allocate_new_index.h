#pragma once

#include "../data_structures/dchain.h"
#include "../internals/internals.h"

namespace bdd {
namespace emulation {

inline void __dchain_allocate_new_index(const BDD &bdd, const Call *call_node,
                                        pkt_t &pkt, time_ns_t time,
                                        state_t &state, meta_t &meta,
                                        context_t &ctx, const cfg_t &cfg) {
  auto call = call_node->get_call();

  assert(!call.args["chain"].expr.isNull());
  assert(!call.args["time"].expr.isNull());
  assert(!call.args["index_out"].out.isNull());

  auto addr_expr = call.args["chain"].expr;
  auto index_out_expr = call.args["index_out"].out;

  auto addr = kutil::expr_addr_to_obj_addr(addr_expr);

  auto generated_symbols =
      call_node->get_locally_generated_symbols({"out_of_space"});
  assert(generated_symbols.size() == 1 && "Expected one symbol");
  const auto &out_of_space_symbol = *generated_symbols.begin();

  auto ds_dchain = state.get(addr);
  auto dchain = Dchain::cast(ds_dchain);

  uint32_t index_out;
  auto out_of_space = !dchain->allocate_new_index(index_out, time);

  concretize(ctx, out_of_space_symbol.expr, out_of_space);

  if (!out_of_space) {
    concretize(ctx, index_out_expr, index_out);
  }

  meta.dchain_allocations++;
}

inline std::pair<std::string, operation_ptr> dchain_allocate_new_index() {
  return {"dchain_allocate_new_index", __dchain_allocate_new_index};
}

} // namespace emulation
} // namespace bdd