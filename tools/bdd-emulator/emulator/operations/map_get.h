#pragma once

#include "../data_structures/map.h"
#include "../internals/internals.h"

namespace bdd {
namespace emulation {

inline void __map_get(const BDD &bdd, const Call *call_node, pkt_t &pkt,
                      time_ns_t time, state_t &state, meta_t &meta,
                      context_t &ctx, const cfg_t &cfg) {
  auto call = call_node->get_call();

  assert(!call.args["map"].expr.isNull());
  assert(!call.args["key"].in.isNull());
  assert(!call.args["value_out"].out.isNull());

  auto addr_expr = call.args["map"].expr;
  auto key_expr = call.args["key"].in;
  auto value_out_expr = call.args["value_out"].out;

  auto generated_symbols =
      call_node->get_locally_generated_symbols({"map_has_this_key"});
  assert(generated_symbols.size() == 1 && "Expected one symbol");
  const auto &map_has_this_key_symbol = *generated_symbols.begin();

  auto addr = kutil::expr_addr_to_obj_addr(addr_expr);
  auto key = bytes_from_expr(key_expr, ctx);

  auto ds_map = state.get(addr);
  auto map = Map::cast(ds_map);

  int value;
  auto contains = map->get(key, value);

  if (contains) {
    concretize(ctx, value_out_expr, value);
  }

  concretize(ctx, map_has_this_key_symbol.expr, contains);
}

inline std::pair<std::string, operation_ptr> map_get() {
  return {"map_get", __map_get};
}

} // namespace emulation
} // namespace bdd