#include "bdd-analyzer-report.h"

#include <fstream>

#include <nlohmann/json.hpp>

using json = nlohmann::json;

void from_json(const json &j, bdd_profile_t::config_t &config) {
  for (const auto &dev_pcap : j["pcaps"]) {
    dev_pcap_t elem;
    dev_pcap.at("device").get_to(elem.device);
    dev_pcap.at("pcap").get_to(elem.pcap);
    dev_pcap.at("warmup").get_to(elem.warmup);
    config.pcaps.push_back(elem);
  }
}

void from_json(const json &j, bdd_profile_t::meta_t &meta) {
  j.at("total_packets").get_to(meta.total_packets);
  j.at("total_bytes").get_to(meta.total_bytes);
  j.at("avg_pkt_size").get_to(meta.avg_pkt_size);
}

void from_json(const json &j, bdd_profile_t::map_stats_t &map_stats) {
  j.at("node").get_to(map_stats.node);
  j.at("total_packets").get_to(map_stats.total_packets);
  j.at("total_flows").get_to(map_stats.total_flows);
  j.at("avg_pkts_per_flow").get_to(map_stats.avg_pkts_per_flow);
  j.at("packets_per_flow").get_to(map_stats.packets_per_flow);
}

void from_json(const json &j,
               std::unordered_map<bdd::node_id_t, uint64_t> &counters) {
  for (const auto &kv : j.items()) {
    bdd::node_id_t node_id = std::stoull(kv.key());
    uint64_t count = kv.value();
    counters[node_id] = count;
  }
}

void from_json(const json &j, bdd_profile_t &report) {
  j.at("config").get_to(report.config);
  j.at("meta").get_to(report.meta);
  j.at("map_stats").get_to(report.map_stats);

  // Use our parser instead of the default one provided by the library. Their
  // one is not working for some reason.
  from_json(j["counters"], report.counters);
}

bdd_profile_t parse_bdd_profile(const std::string &filename) {
  std::ifstream file(filename);
  if (!file.is_open()) {
    assert(false && "failed to open file");
  }

  json j = json::parse(file);
  bdd_profile_t report = j.get<bdd_profile_t>();

  return report;
}