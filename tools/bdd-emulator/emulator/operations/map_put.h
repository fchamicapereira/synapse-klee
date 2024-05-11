#pragma once

#include "../data_structures/map.h"
#include "../internals/internals.h"

namespace bdd {
namespace emulation {

inline void __map_put(const BDD &bdd, const Call *call_node, pkt_t &pkt,
                      time_ns_t time, state_t &state, meta_t &meta,
                      context_t &ctx, const cfg_t &cfg) {
  auto call = call_node->get_call();

  assert(!call.args["map"].expr.isNull());
  assert(!call.args["key"].in.isNull());
  assert(!call.args["value"].expr.isNull());

  auto addr_expr = call.args["map"].expr;
  auto key_expr = call.args["key"].in;
  auto value_expr = call.args["value"].expr;

  auto addr = kutil::expr_addr_to_obj_addr(addr_expr);
  auto value = kutil::solver_toolbox.value_from_expr(value_expr, ctx);
  auto key = bytes_from_expr(key_expr, ctx);

  auto ds_map = state.get(addr);
  auto map = Map::cast(ds_map);

  map->put(key, value);
}

inline std::pair<std::string, operation_ptr> map_put() {
  return {"map_put", __map_put};
}

} // namespace emulation
} // namespace bdd