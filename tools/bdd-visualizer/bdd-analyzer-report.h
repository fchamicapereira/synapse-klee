#pragma once

#include <stdbool.h>
#include <stdint.h>

#include <string>
#include <unordered_map>
#include <vector>

#include "call-paths-to-bdd.h"

struct config_t {
  uint16_t device;
  std::string pcap;
  uint64_t total_packets;
  uint64_t total_flows;
  uint64_t churn_fpm;
  uint64_t rate_pps;
  uint16_t packet_sizes;
  bool traffic_uniform;
  bool traffic_zipf;
  float traffic_zipf_param;
};

typedef std::unordered_map<bdd::node_id_t, uint64_t> bdd_node_counters;
typedef uint64_t time_ns_t;

struct bdd_profile_t {
  config_t config;
  bdd_node_counters counters;
  time_ns_t elapsed;
  int avg_pkt_bytes;
};

bdd_profile_t parse_bdd_profile(const std::string &filename);