#pragma once

#include "../internals.h"

namespace BDD {
namespace emulation {

inline void __nf_set_rte_ipv4_tcpudp_checksum(const Call *call_node, pkt_t &pkt,
                                              time_ns_t time, state_t &state,
                                              context_t &ctx,
                                              const cfg_t &cfg) {
  // Nothing to do here.
}

inline std::pair<std::string, operation_ptr> nf_set_rte_ipv4_tcpudp_checksum() {
  return {symbex::FN_SET_CHECKSUM, __nf_set_rte_ipv4_tcpudp_checksum};
}

} // namespace emulation
} // namespace BDD