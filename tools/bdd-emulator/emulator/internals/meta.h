#pragma once

#include "call-paths-to-bdd.h"
#include "klee-util.h"

#include "base-types.h"

#include <stdint.h>
#include <unordered_map>

namespace BDD {
namespace emulation {

struct meta_t {
  std::unordered_map<node_id_t, uint64_t> hit_counter;
  uint64_t packet_counter;
  uint64_t accepted;
  uint64_t rejected;
  time_ns_t elapsed;
  uint64_t flows_expired;
  uint64_t dchain_allocations;

  meta_t() { reset(); }

  void reset() {
    hit_counter.clear();
    packet_counter = 0;
    accepted = 0;
    rejected = 0;
    elapsed = 0;
    flows_expired = 0;
    dchain_allocations = 0;
  }

  std::unordered_map<node_id_t, emulation::hit_rate_t> get_hit_rate() const {
    std::unordered_map<node_id_t, emulation::hit_rate_t> hit_rate;

    for (auto it = hit_counter.begin(); it != hit_counter.end(); it++) {
      if (packet_counter > 0) {
        hit_rate[it->first] = it->second / (float)(packet_counter);
      } else {
        hit_rate[it->first] = 0;
      }
    }

    return hit_rate;
  }
};

inline std::ostream &operator<<(std::ostream &os, const meta_t &meta) {
  auto hit_rate = meta.get_hit_rate();
  auto churn_fpm = (60.0 * meta.flows_expired) / (double)(meta.elapsed * 1e-9);

  os << "Meta:\n";
  os << "  packets:       " << meta.packet_counter << "\n";
  os << "  accepted:      " << meta.accepted << " ("
     << (100.0 * meta.accepted / meta.packet_counter) << "%)\n";
  os << "  rejected:      " << meta.rejected << " ("
     << (100.0 * meta.rejected / meta.packet_counter) << "%)\n";
  os << "  elapsed time:  " << meta.elapsed << " ns ("
     << (double)(meta.elapsed) * 1e-9 << " s)\n";
  os << "  flows expired: " << meta.flows_expired << " (" << churn_fpm
     << " fpm)\n";
  os << "  dchain allocs: " << meta.dchain_allocations << "\n";

  os << "Hit rate (per node):\n";
  for (auto it = hit_rate.begin(); it != hit_rate.end(); it++) {
    os << "  " << it->first << " \t " << 100 * it->second << "%\n";
  }

  return os;
}

} // namespace emulation
} // namespace BDD