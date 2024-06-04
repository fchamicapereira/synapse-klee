#pragma once

#include "parser.h"
#include "simple_placer.h"

#include <unordered_map>

namespace synapse {
namespace tofino {

enum class TNAVersion { TNA1, TNA2 };

struct TNAConstraints {
  int max_packet_bytes_in_condition;
  int stages;
  bits_t sram_per_stage;
  bits_t tcam_per_stage;
  int max_logical_tcam_tables;
  int max_logical_sram_and_tcam_tables;
  bits_t phv_size;
  int phv_8bit_containers;
  int phv_16bit_containers;
  int phv_32bit_containers;
  bits_t packet_buffer_size;
  bits_t exact_match_xbar;
  int max_exact_match_keys;
  bits_t ternary_match_xbar;
  int max_ternary_match_keys;
};

class TNA {
private:
  const TNAVersion version;
  const TNAConstraints constraints;

  SimplePlacer simple_placer;

public:
  Parser parser;

  TNA(TNAVersion version);
  TNA(const TNA &other);

  // Tofino compiler complains if we access more than 4 bytes of the packet on
  // the same if statement.
  bool condition_meets_phv_limit(klee::ref<klee::Expr> expr) const;

  void place_table(const Table *table,
                   const std::unordered_set<DS_ID> &dependencies);

  PlacementStatus
  can_place_table(const Table *table,
                  const std::unordered_set<DS_ID> &dependencies) const;

  void log_debug_placement() const;
};

} // namespace tofino
} // namespace synapse