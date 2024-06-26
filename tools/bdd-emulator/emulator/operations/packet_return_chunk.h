#pragma once

#include "../internals/internals.h"

namespace bdd {
namespace emulation {

inline void __packet_return_chunk(const BDD &bdd, const Call *call_node,
                                  pkt_t &pkt, time_ns_t time, state_t &state,
                                  meta_t &meta, context_t &ctx,
                                  const cfg_t &cfg) {
  // Nothing relevant to do here
}

inline std::pair<std::string, operation_ptr> packet_return_chunk() {
  return {"packet_return_chunk", __packet_return_chunk};
}

} // namespace emulation
} // namespace bdd