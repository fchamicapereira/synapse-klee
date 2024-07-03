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
  std::vector<double> fractions;

  // Fraction of traffic being steered to another recirculation port.
  // This avoids us double counting the contribution of recirculation traffic to
  // the final throughput estimation.
  double steering_fraction;
};

class PerfOracle {
private:
  int total_ports;
  uint64_t port_capacity_bps;
  int total_recirc_ports;
  uint64_t recirc_port_capacity_bps;
  int avg_pkt_bytes;

  std::vector<RecircPortUsage> recirc_ports_usage;
  double non_recirc_traffic;

  uint64_t throughput_pps;

public:
  PerfOracle(const TNAProperties *properties, int avg_pkt_bytes);
  PerfOracle(const PerfOracle &other);

  void add_recirculated_traffic(int port, int port_recirculations,
                                double fraction,
                                std::optional<int> prev_recirc_port);
  uint64_t estimate_throughput_pps() const;
  void log_debug() const;

private:
  void steer_recirculation_traffic(int source_port, int destination_port,
                                   double fraction);
  void update_estimate_throughput_pps();
};

} // namespace tofino
} // namespace synapse