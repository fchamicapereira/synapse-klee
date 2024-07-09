#include "perf_oracle.h"

#include "tna.h"

#include <cmath>

#define NEWTON_MAX_ITERATIONS 10
#define NEWTON_PRECISION 1e-3

namespace synapse {
namespace tofino {

PerfOracle::PerfOracle(const TNAProperties *properties, int _avg_pkt_bytes)
    : total_ports(properties->total_ports),
      port_capacity_bps(properties->port_capacity_bps),
      total_recirc_ports(properties->total_recirc_ports),
      recirc_port_capacity_bps(properties->recirc_port_capacity_bps),
      avg_pkt_bytes(_avg_pkt_bytes),
      recirc_ports_usage(properties->total_recirc_ports), non_recirc_traffic(1),
      throughput_pps(port_capacity_bps * total_ports) {
  for (int port = 0; port < total_recirc_ports; port++) {
    recirc_ports_usage[port].port = port;
    recirc_ports_usage[port].steering_fraction = 0;
  }

  update_estimate_throughput_pps();
}

PerfOracle::PerfOracle(const PerfOracle &other)
    : total_ports(other.total_ports),
      port_capacity_bps(other.port_capacity_bps),
      total_recirc_ports(other.total_recirc_ports),
      recirc_port_capacity_bps(other.recirc_port_capacity_bps),
      avg_pkt_bytes(other.avg_pkt_bytes),
      recirc_ports_usage(other.recirc_ports_usage),
      non_recirc_traffic(other.non_recirc_traffic),
      throughput_pps(other.throughput_pps) {}

static bool fractions_le(double f0, double f1) {
  double delta = f0 - f1;
  double epsilon = 1e-8;
  return delta < epsilon;
}

void PerfOracle::add_recirculated_traffic(int port, int port_recirculations,
                                          double fraction,
                                          std::optional<int> prev_recirc_port) {
  assert(port < total_recirc_ports);
  assert(port_recirculations > 0);
  assert(fraction >= 0);
  assert(fraction <= 1);

  RecircPortUsage &usage = recirc_ports_usage[port];
  assert(usage.port == port);

  int curr_port_recirculations = usage.fractions.size();

  assert(port_recirculations <= curr_port_recirculations + 1);

  if (port_recirculations > curr_port_recirculations) {
    usage.fractions.push_back(0);

    assert(
        curr_port_recirculations == 0 ||
        fractions_le(fraction, usage.fractions[curr_port_recirculations - 1]));
    assert(port_recirculations == (int)usage.fractions.size());

    if (curr_port_recirculations > 0) {
      // double shenanigans to avoid floating point precision issues
      double last_fraction = usage.fractions[curr_port_recirculations - 1];
      fraction = std::min(fraction, last_fraction);
    }
  }

  usage.fractions[port_recirculations - 1] += fraction;

  assert(usage.fractions[port_recirculations - 1] >= 0);
  assert(usage.fractions[port_recirculations - 1] <= 1);

  if (prev_recirc_port.has_value() && *prev_recirc_port != port) {
    steer_recirculation_traffic(*prev_recirc_port, port, fraction);
  } else if (non_recirc_traffic > 0) {
    non_recirc_traffic -= fraction;
    assert(non_recirc_traffic >= 0);
    assert(non_recirc_traffic <= 1);
  }

  update_estimate_throughput_pps();
}

void PerfOracle::steer_recirculation_traffic(int source_port,
                                             int destination_port,
                                             double fraction) {
  assert(source_port < total_recirc_ports);
  assert(destination_port < total_recirc_ports);
  assert(fraction >= 0);
  assert(fraction <= 1);

  RecircPortUsage &source_usage = recirc_ports_usage[source_port];
  RecircPortUsage &destination_usage = recirc_ports_usage[destination_port];

  assert(source_usage.port == source_port);
  assert(destination_usage.port == destination_port);

  source_usage.steering_fraction += fraction;

  assert(source_usage.steering_fraction >= 0);
  assert(source_usage.steering_fraction <= 1);

  assert(destination_usage.steering_fraction >= 0);
  assert(destination_usage.steering_fraction <= 1);
}

static uint64_t single_recirc_estimate(uint64_t Tin, uint64_t Cp) {
  uint64_t Tout = std::min(Tin, Cp);
  return Tout;
}

static uint64_t double_recirc_estimate(uint64_t Tin, uint64_t Cr, uint64_t Cp,
                                       double s) {
  uint64_t Ts = (-Tin + sqrt(Tin * Tin + 4 * Cr * Tin * s)) / 2;
  uint64_t Tout_in = (Tin / (double)(Tin + Ts)) * Cr * (1.0 - s);
  uint64_t Tout_s = (Ts / (double)(Tin + Ts)) * Cr;
  uint64_t Tout = Tout_in + Tout_s;
  return std::min(std::min(Tin, Cp), Tout);
}

// Coefficients are in increasing order: x^0, x^1, x^2, ...
// min and max are inclusive
static double newton_root_finder(double *coefficients, int n_coefficients,
                                 uint64_t min, uint64_t max) {
  double x = (min + max) / 2.0;
  int it = 0;

  while (it < NEWTON_MAX_ITERATIONS) {
    double f = 0;
    double df_dx = 0;

    // It's very important to compute each term in this order, otherwise we
    // don't converge (precision loss)
    for (int c = n_coefficients; c >= 0; c--) {
      f += coefficients[c] * pow(x, c);

      if (c > 0) {
        df_dx += c * coefficients[c] * pow(x, c - 1);
      }
    }

    if (std::abs(f) <= NEWTON_PRECISION) {
      break;
    }

    x = x - f / df_dx;
    it++;
  }

  return x;
}

static uint64_t triple_recirc_estimate(uint64_t Tin, uint64_t Cr, uint64_t Cp,
                                       double s0, double s1) {
  double a = (s1 / s0) * (1.0 / Tin);
  double b = 1;
  double c = Tin;
  double d = -1.0 * Tin * Cr * s0;

  double Ts0_coefficients[] = {d, c, b, a};
  double Ts0 = newton_root_finder(Ts0_coefficients, 4, 0, Cr);
  double Ts1 = Ts0 * Ts0 * (1.0 / Tin) * (s1 / s0);

  double Tout_in = (Tin / (double)(Tin + Ts0 + Ts1)) * Cr * (1.0 - s0);
  double Tout_s0 = (Ts0 / (double)(Tin + Ts0 + Ts1)) * Cr * (1.0 - s1);
  double Tout_s1 = (Ts1 / (double)(Tin + Ts0 + Ts1)) * Cr;

  uint64_t Tout = Tout_in + Tout_s0 + Tout_s1;

  return std::min(std::min(Tin, Cp), Tout);
}

void PerfOracle::update_estimate_throughput_pps() {
  uint64_t Tswitch_bps = port_capacity_bps * total_ports;
  uint64_t throughput_bps = 0;

  for (const RecircPortUsage &usage : recirc_ports_usage) {
    size_t recirc_depth = usage.fractions.size();

    if (recirc_depth == 0) {
      continue;
    }

    uint64_t Tin_bps = Tswitch_bps * usage.fractions[0];
    uint64_t Tsteering_bps = Tswitch_bps * usage.steering_fraction;
    uint64_t Tout_bps = 0;

    switch (recirc_depth) {
    case 1: {
      // Single recirculation, easy
      Tout_bps = single_recirc_estimate(Tin_bps, port_capacity_bps);
    } break;
    case 2: {
      // Recirculation surplus
      // s is relative to the first recirculation

      assert(fractions_le(usage.fractions[1], usage.fractions[0]));
      assert(usage.fractions[0] > 0);

      double s = usage.fractions[1] / usage.fractions[0];

      Tout_bps = double_recirc_estimate(Tin_bps, recirc_port_capacity_bps,
                                        port_capacity_bps, s);
    } break;
    case 3: {
      // s1 is relative to s0
      // s0 is relative to the first recirculation

      assert(fractions_le(usage.fractions[2], usage.fractions[1]));
      assert(fractions_le(usage.fractions[1], usage.fractions[0]));

      assert(usage.fractions[1] > 0);
      assert(usage.fractions[0] > 0);

      double s0 = usage.fractions[1] / usage.fractions[0];
      double s1 = usage.fractions[2] / usage.fractions[1];

      Tout_bps = triple_recirc_estimate(Tin_bps, recirc_port_capacity_bps,
                                        port_capacity_bps, s0, s1);
    } break;
    default: {
      assert(false && "TODO");
    }
    }

    Tsteering_bps = std::min(Tsteering_bps, Tout_bps);
    throughput_bps += Tout_bps - Tsteering_bps;
  }

  throughput_bps += non_recirc_traffic * Tswitch_bps;

  uint64_t old_estimate_pps = throughput_pps;
  uint64_t new_estimate_pps = throughput_bps / (avg_pkt_bytes * 8);

  assert(new_estimate_pps <= old_estimate_pps);

  throughput_pps = new_estimate_pps;
}

uint64_t PerfOracle::estimate_throughput_pps() const { return throughput_pps; }

void PerfOracle::log_debug() const {
  Log::dbg() << "====== PerfOracle ======\n";
  Log::dbg() << "Non recirculated: " << non_recirc_traffic << "\n";
  Log::dbg() << "Recirculations:\n";
  for (const RecircPortUsage &usage : recirc_ports_usage) {
    Log::dbg() << "  Port " << usage.port << ":";
    for (size_t i = 0; i < usage.fractions.size(); i++) {
      Log::dbg() << " " << usage.fractions[i];
    }
    Log::dbg() << " (steering=" << usage.steering_fraction << ")";
    Log::dbg() << "\n";
  }
  Log::dbg() << "Estimate: " << estimate_throughput_pps() << " pps\n";
  Log::dbg() << "========================\n";
}

} // namespace tofino
} // namespace synapse