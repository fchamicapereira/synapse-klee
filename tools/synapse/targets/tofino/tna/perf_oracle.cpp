#include "perf_oracle.h"

#include "tna.h"

#include <cmath>

namespace synapse {
namespace tofino {

PerfOracle::PerfOracle(const TNAProperties *properties, int _avg_pkt_bytes)
    : total_ports(properties->total_ports),
      port_capacity_bps(properties->port_capacity_bps),
      total_recirc_ports(properties->total_recirc_ports),
      recirc_port_capacity_bps(properties->recirc_port_capacity_bps),
      avg_pkt_bytes(_avg_pkt_bytes),
      recirc_ports_usage(properties->total_recirc_ports), non_recirc_traffic(1),
      cached_throughput_kpps(std::nullopt) {
  for (int port = 0; port < total_recirc_ports; port++) {
    recirc_ports_usage[port].port = port;
  }
}

PerfOracle::PerfOracle(const PerfOracle &other)
    : total_ports(other.total_ports),
      port_capacity_bps(other.port_capacity_bps),
      total_recirc_ports(other.total_recirc_ports),
      recirc_port_capacity_bps(other.recirc_port_capacity_bps),
      avg_pkt_bytes(other.avg_pkt_bytes),
      recirc_ports_usage(other.recirc_ports_usage),
      non_recirc_traffic(other.non_recirc_traffic),
      cached_throughput_kpps(other.cached_throughput_kpps) {}

void PerfOracle::add_recirculated_traffic(int port, int total_recirc_times,
                                          float fraction) {
  cached_throughput_kpps.reset();

  assert(port < total_recirc_ports);
  RecircPortUsage &usage = recirc_ports_usage[port];

  assert(usage.port == port);

  int curr_total_recirc_times = usage.fractions.size();
  assert(total_recirc_times == curr_total_recirc_times ||
         total_recirc_times == curr_total_recirc_times + 1);

  if (total_recirc_times == curr_total_recirc_times + 1) {
    usage.fractions.push_back(0);
  }

  usage.fractions[total_recirc_times - 1] += fraction;

  if (total_recirc_times == 1 && non_recirc_traffic > 0) {
    non_recirc_traffic -= fraction;
    assert(non_recirc_traffic >= 0);
    assert(non_recirc_traffic <= 1);
  }

  assert(usage.fractions[total_recirc_times - 1] <= 1);

  log_debug();
}

uint64_t PerfOracle::estimate_throughput_kpps() const {
  if (cached_throughput_kpps.has_value()) {
    return cached_throughput_kpps.value();
  }

  uint64_t Tswitch_bps = port_capacity_bps * total_ports;
  uint64_t total_throughput_bps = 0;

  for (const RecircPortUsage &usage : recirc_ports_usage) {
    size_t recirc_depth = usage.fractions.size();

    if (recirc_depth == 0) {
      continue;
    }

    uint64_t Tin_bps = Tswitch_bps * usage.fractions[0];
    uint64_t Tout_bps = 0;

    if (recirc_depth == 1) {
      // Single recirculation, easy
      Tout_bps = Tin_bps;
    } else if (recirc_depth == 2) {
      // Recirculation surplus

      // s is relative to the first recirculation
      assert(usage.fractions[1] <= usage.fractions[0]);
      assert(usage.fractions[0] > 0);
      float s = usage.fractions[1] / usage.fractions[0];

      uint64_t Ts_bps =
          (-Tin_bps + sqrt(Tin_bps * Tin_bps +
                           4 * recirc_port_capacity_bps * Tin_bps * s)) /
          2;

      uint64_t Tout_in_bps = (Tin_bps / (float)(Tin_bps + Ts_bps)) *
                             recirc_port_capacity_bps * (1.0 - s);
      uint64_t Tout_s_bps =
          (Ts_bps / (float)(Tin_bps + Ts_bps)) * recirc_port_capacity_bps;

      Tout_bps = std::min(Tin_bps, Tout_in_bps + Tout_s_bps);
    } else {
      assert(false && "TODO");
    }

    total_throughput_bps += std::min(Tout_bps, port_capacity_bps);
  }

  total_throughput_bps += non_recirc_traffic * Tswitch_bps;

  uint64_t total_throughput_kpps =
      total_throughput_bps / (avg_pkt_bytes * 8 * 1000);

  cached_throughput_kpps = total_throughput_kpps;

  return total_throughput_kpps;
}

void PerfOracle::log_debug() const {
  Log::dbg() << "====== PerfOracle ======\n";
  Log::dbg() << "Recirculations:\n";
  for (const RecircPortUsage &usage : recirc_ports_usage) {
    Log::dbg() << "  Port " << usage.port << ":";
    for (size_t i = 0; i < usage.fractions.size(); i++) {
      Log::dbg() << " " << usage.fractions[i];
    }
    Log::dbg() << "\n";
  }
  Log::dbg() << "Estimate: " << estimate_throughput_kpps() << " kpps\n";
  Log::dbg() << "========================\n";
}

} // namespace tofino
} // namespace synapse