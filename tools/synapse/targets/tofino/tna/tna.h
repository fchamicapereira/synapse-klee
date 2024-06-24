#pragma once

#include "parser.h"
#include "simple_placer.h"
#include "perf_oracle.h"

#include <unordered_map>

namespace synapse {
namespace tofino {

enum class TNAVersion { TNA1, TNA2 };

struct TNAProperties {
  uint64_t port_capacity_bps;
  int total_ports;
  uint64_t recirc_port_capacity_bps;
  int total_recirc_ports;
  int max_packet_bytes_in_condition;
  int pipes;
  int stages;
  bits_t sram_per_stage;
  bits_t tcam_per_stage;
  bits_t map_ram_per_stage;
  int max_logical_tcam_tables_per_stage;
  int max_logical_sram_and_tcam_tables_per_stage;
  bits_t phv_size;
  int phv_8bit_containers;
  int phv_16bit_containers;
  int phv_32bit_containers;
  bits_t packet_buffer_size;
  bits_t exact_match_xbar_per_stage;
  int max_exact_match_keys;
  bits_t ternary_match_xbar;
  int max_ternary_match_keys;
  bits_t max_salu_size;
};

class TNA {
private:
  const TNAVersion version;
  const TNAProperties properties;

  SimplePlacer simple_placer;
  PerfOracle perf_oracle;

public:
  Parser parser;

  TNA(TNAVersion version, int avg_pkt_bytes);
  TNA(const TNA &other);

  TNAVersion get_version() const { return version; }
  const TNAProperties &get_properties() const { return properties; }

  // Tofino compiler complains if we access more than 4 bytes of the packet on
  // the same if statement.
  bool condition_meets_phv_limit(klee::ref<klee::Expr> expr) const;

  void place(const DS *ds, const std::unordered_set<DS_ID> &deps);
  void place_many(const std::vector<std::unordered_set<DS *>> &ds,
                  const std::unordered_set<DS_ID> &deps);

  PlacementStatus can_place(const DS *ds,
                            const std::unordered_set<DS_ID> &deps) const;
  PlacementStatus
  can_place_many(const std::vector<std::unordered_set<DS *>> &ds,
                 const std::unordered_set<DS_ID> &deps) const;

  void log_debug_placement() const;

  const PerfOracle &get_perf_oracle() const;
  PerfOracle &get_mutable_perf_oracle();
};

} // namespace tofino
} // namespace synapse