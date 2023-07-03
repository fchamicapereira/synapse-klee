#pragma once

#include "../internals/internals.h"

namespace BDD {
namespace emulation {

inline void __current_time(const BDD& bdd, const Call *call_node, pkt_t &pkt, time_ns_t time,
                           state_t &state, meta_t &meta, context_t &ctx,
                           const cfg_t &cfg) {
  auto call = call_node->get_call();

  assert(!call.ret.isNull());
  auto _time = call.ret;

  concretize(ctx, _time, time);
}

inline std::pair<std::string, operation_ptr> current_time() {
  return {symbex::FN_CURRENT_TIME, __current_time};
}

} // namespace emulation
} // namespace BDD