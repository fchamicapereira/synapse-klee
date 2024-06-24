#pragma once

#include <cstdint>
#include <vector>
#include <optional>

namespace synapse {
namespace tofino {

struct TNAProperties;

struct RecircPortUsage {
  int port;

  // One for each consecutive recirculation.
  // E.g.:
  //    Index 0 contains the fraction of traffic recirculated once.
  //    Index 1 contains the fraction of traffic recirculated twice.
  //    Etc.
  std::vector<float> fractions;
};

class PerfOracle {
private:
  int total_ports;
  uint64_t port_capacity_bps;
  int total_recirc_ports;
  uint64_t recirc_port_capacity_bps;
  int avg_pkt_bytes;

  std::vector<RecircPortUsage> recirc_ports_usage;
  float non_recirc_traffic;

  uint64_t throughput_kpps;

public:
  PerfOracle(const TNAProperties *properties, int avg_pkt_bytes);
  PerfOracle(const PerfOracle &other);

  void add_recirculated_traffic(int port, int total_recirc_times,
                                float fraction);
  uint64_t estimate_throughput_kpps() const;
  void log_debug() const;

private:
  void update_estimate_throughput_kpps();
};

} // namespace tofino
} // namespace synapse