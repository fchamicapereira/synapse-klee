#pragma once

#include <stdbool.h>
#include <stdint.h>

#include <string>
#include <unordered_map>
#include <vector>

#include "call-paths-to-bdd.h"

struct dev_pcap_t {
  uint16_t device;
  std::string pcap;
  bool warmup;
};

typedef uint64_t time_ns_t;

struct bdd_profile_t {
  struct config_t {
    std::vector<dev_pcap_t> pcaps;
  } config;

  struct meta_t {
    uint64_t total_packets;
    uint64_t total_bytes;
    int avg_pkt_size;
  } meta;

  struct map_stats_t {
    bdd::node_id_t node;
    uint64_t total_packets;
    uint64_t total_flows;
    uint64_t avg_pkts_per_flow;
    std::vector<uint64_t> packets_per_flow;
  };

  std::vector<map_stats_t> map_stats;

  std::unordered_map<bdd::node_id_t, uint64_t> counters;
};

bdd_profile_t parse_bdd_profile(const std::string &filename);