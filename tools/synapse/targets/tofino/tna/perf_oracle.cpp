#include "perf_oracle.h"

#include "tna.h"

#include <cmath>

#define NEWTON_MAX_ITERATIONS 100
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
      throughput_kpps(0) {
  for (int port = 0; port < total_recirc_ports; port++) {
    recirc_ports_usage[port].port = port;
  }

  update_estimate_throughput_kpps();
}

PerfOracle::PerfOracle(const PerfOracle &other)
    : total_ports(other.total_ports),
      port_capacity_bps(other.port_capacity_bps),
      total_recirc_ports(other.total_recirc_ports),
      recirc_port_capacity_bps(other.recirc_port_capacity_bps),
      avg_pkt_bytes(other.avg_pkt_bytes),
      recirc_ports_usage(other.recirc_ports_usage),
      non_recirc_traffic(other.non_recirc_traffic),
      throughput_kpps(other.throughput_kpps) {}

void PerfOracle::add_recirculated_traffic(int port, int port_recirculations,
                                          int total_recirculations,
                                          float fraction) {
  assert(port < total_recirc_ports);
  RecircPortUsage &usage = recirc_ports_usage[port];

  assert(usage.port == port);

  int curr_port_recirculations = usage.fractions.size();
  assert(port_recirculations == curr_port_recirculations ||
         port_recirculations == curr_port_recirculations + 1);

  if (port_recirculations > curr_port_recirculations) {
    usage.fractions.push_back(0);
    assert(curr_port_recirculations == 0 ||
           fraction <= usage.fractions[curr_port_recirculations - 1]);
    assert(port_recirculations == (int)usage.fractions.size());
  }

  usage.fractions[port_recirculations - 1] += fraction;

  assert(usage.fractions[port_recirculations - 1] >= 0);
  assert(usage.fractions[port_recirculations - 1] <= 1);

  if (total_recirculations == 1 && non_recirc_traffic > 0) {
    non_recirc_traffic -= fraction;
    assert(non_recirc_traffic >= 0);
    assert(non_recirc_traffic <= 1);
  }

  update_estimate_throughput_kpps();
}

static uint64_t single_recirc_estimate(uint64_t Tin, uint64_t Cp) {
  uint64_t Tout = std::min(Tin, Cp);
  return Tout;
}

static uint64_t double_recirc_estimate(uint64_t Tin, uint64_t Cr, uint64_t Cp,
                                       float s) {
  uint64_t Ts = (-Tin + sqrt(Tin * Tin + 4 * Cr * Tin * s)) / 2;
  uint64_t Tout_in = (Tin / (float)(Tin + Ts)) * Cr * (1.0 - s);
  uint64_t Tout_s = (Ts / (float)(Tin + Ts)) * Cr;
  uint64_t Tout = Tout_in + Tout_s;
  return std::min(std::min(Tin, Cp), Tout);
}

// Coefficients are in increasing order: x^0, x^1, x^2, ...
// min and max are inclusive
static float newton_root_finder(double *coefficients, int n_coefficients,
                                uint64_t min, uint64_t max) {
  double x = (min + max) / 2.0;
  int it = 0;

  while (1) {
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

    assert(it < NEWTON_MAX_ITERATIONS);
  }

  return x;
}

static uint64_t triple_recirc_estimate(uint64_t Tin, uint64_t Cr, uint64_t Cp,
                                       float s0, float s1) {
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

void PerfOracle::update_estimate_throughput_kpps() {
  uint64_t Tswitch_bps = port_capacity_bps * total_ports;
  uint64_t throughput_bps = 0;

  for (const RecircPortUsage &usage : recirc_ports_usage) {
    size_t recirc_depth = usage.fractions.size();

    if (recirc_depth == 0) {
      continue;
    }

    uint64_t Tin_bps = Tswitch_bps * usage.fractions[0];
    uint64_t Tout_bps = 0;

    switch (recirc_depth) {
    case 1: {
      // Single recirculation, easy
      Tout_bps = single_recirc_estimate(Tin_bps, port_capacity_bps);
    } break;
    case 2: {
      // Recirculation surplus
      // s is relative to the first recirculation

      assert(usage.fractions[1] <= usage.fractions[0]);
      assert(usage.fractions[0] > 0);

      float s = usage.fractions[1] / usage.fractions[0];

      Tout_bps = double_recirc_estimate(Tin_bps, recirc_port_capacity_bps,
                                        port_capacity_bps, s);
    } break;
    case 3: {
      // s1 is relative to s0
      // s0 is relative to the first recirculation

      assert(usage.fractions[2] <= usage.fractions[1]);
      assert(usage.fractions[1] <= usage.fractions[0]);

      assert(usage.fractions[1] > 0);
      assert(usage.fractions[0] > 0);

      float s0 = usage.fractions[1] / usage.fractions[0];
      float s1 = usage.fractions[2] / usage.fractions[1];

      Tout_bps = triple_recirc_estimate(Tin_bps, recirc_port_capacity_bps,
                                        port_capacity_bps, s0, s1);
    } break;
    default: {
      assert(false && "TODO");
    }
    }

    throughput_bps += Tout_bps;
  }

  throughput_bps += non_recirc_traffic * Tswitch_bps;
  throughput_kpps = throughput_bps / (avg_pkt_bytes * 8 * 1000);
}

uint64_t PerfOracle::estimate_throughput_kpps() const {
  return throughput_kpps;
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