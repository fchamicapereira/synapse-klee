#pragma once

#include "../internals/internals.h"

namespace bdd {
namespace emulation {

inline void __packet_borrow_next_chunk(const BDD &bdd, const Call *call_node,
                                       pkt_t &pkt, time_ns_t time,
                                       state_t &state, meta_t &meta,
                                       context_t &ctx, const cfg_t &cfg) {
  auto call = call_node->get_call();

  assert(!call.extra_vars["the_chunk"].second.isNull());
  assert(!call.args["length"].expr.isNull());

  auto chunk_expr = call.extra_vars["the_chunk"].second;
  auto length = call.args["length"].expr;

  assert(length->getKind() == klee::Expr::Kind::Constant);
  auto chunk_size_bytes = kutil::solver_toolbox.value_from_expr(length);

  concretize(ctx, chunk_expr, pkt.data);
  pkt.data += chunk_size_bytes;
}

inline std::pair<std::string, operation_ptr> packet_borrow_next_chunk() {
  return {"packet_borrow_next_chunk", __packet_borrow_next_chunk};
}

} // namespace emulation
} // namespace bdd