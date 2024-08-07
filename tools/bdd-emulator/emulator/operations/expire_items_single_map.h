#pragma once

#include "../data_structures/dchain.h"
#include "../data_structures/map.h"
#include "../data_structures/vector.h"
#include "../internals/internals.h"

namespace bdd {
namespace emulation {

inline time_ns_t get_expiration_time(const BDD &bdd,
                                     klee::ref<klee::Expr> expr) {
  auto time = bdd.get_time();
  auto zero = kutil::solver_toolbox.exprBuilder->Constant(0, 64);
  auto time_eq_0 = kutil::solver_toolbox.exprBuilder->Eq(time.expr, zero);

  klee::ConstraintManager constraints;
  constraints.addConstraint(time_eq_0);

  auto value = kutil::solver_toolbox.signed_value_from_expr(expr, constraints);
  return value * -1;
}

inline void __expire_items_single_map(const BDD &bdd, const Call *call_node,
                                      pkt_t &pkt, time_ns_t time,
                                      state_t &state, meta_t &meta,
                                      context_t &ctx, const cfg_t &cfg) {
  auto call = call_node->get_call();

  assert(!call.args["chain"].expr.isNull());
  assert(!call.args["vector"].expr.isNull());
  assert(!call.args["map"].expr.isNull());
  assert(!call.args["time"].expr.isNull());
  assert(!call.ret.isNull());

  auto dchain_expr = call.args["chain"].expr;
  auto vector_expr = call.args["vector"].expr;
  auto map_expr = call.args["map"].expr;
  auto time_expr = call.args["time"].expr;
  auto number_of_freed_flows_expr = call.ret;

  auto dchain_addr = kutil::expr_addr_to_obj_addr(dchain_expr);
  auto vector_addr = kutil::expr_addr_to_obj_addr(vector_expr);
  auto map_addr = kutil::expr_addr_to_obj_addr(map_expr);

  auto timeout = cfg.timeout.first ? cfg.timeout.second * 1000
                                   : get_expiration_time(bdd, time_expr);
  auto last_time = (time >= timeout) ? time - timeout : 0;

  auto ds_dchain = state.get(dchain_addr);
  auto ds_vector = state.get(vector_addr);
  auto ds_map = state.get(map_addr);

  auto dchain = Dchain::cast(ds_dchain);
  auto vector = Vector::cast(ds_vector);
  auto map = Map::cast(ds_map);

  int count = 0;
  uint32_t index = -1;

  while (dchain->expire_one_index(index, last_time)) {
    auto key = vector->get(index);
    map->erase(key);
    ++count;
  }

  meta.flows_expired += count;
  concretize(ctx, number_of_freed_flows_expr, count);
}

inline std::pair<std::string, operation_ptr> expire_items_single_map() {
  return {"expire_items_single_map", __expire_items_single_map};
}

} // namespace emulation
} // namespace bdd