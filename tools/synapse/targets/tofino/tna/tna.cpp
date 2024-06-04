#include "tna.h"

namespace synapse {
namespace tofino {

static TNAConstraints constraints_from_version(TNAVersion version) {
  TNAConstraints constraints;

  switch (version) {
  case TNAVersion::TNA1:
    constraints = {
        .max_packet_bytes_in_condition = 4,
        .stages = 12,
        .sram_per_stage = 128 * 1024 * 80,
        .tcam_per_stage = 44 * 512 * 24,
        .max_logical_tcam_tables = 8,
        .max_logical_sram_and_tcam_tables = 16,
        .phv_size = 4000,
        .phv_8bit_containers = 64,
        .phv_16bit_containers = 96,
        .phv_32bit_containers = 64,
        .packet_buffer_size = static_cast<bits_t>(20e6 * 8),
        .exact_match_xbar = 128 * 8,
        .max_exact_match_keys = 16,
        .ternary_match_xbar = 66 * 8,
        .max_ternary_match_keys = 8,
    };
    break;
  case TNAVersion::TNA2:
    constraints = {
        .max_packet_bytes_in_condition = 4,
        .stages = 20,
        .sram_per_stage = 128 * 1024 * 80,
        .tcam_per_stage = 44 * 512 * 24,
        .max_logical_tcam_tables = 8,
        .max_logical_sram_and_tcam_tables = 16,
        .phv_size = 5000,
        .phv_8bit_containers = 80,
        .phv_16bit_containers = 120,
        .phv_32bit_containers = 80,
        .packet_buffer_size = static_cast<bits_t>(64e6 * 8),
        .exact_match_xbar = 128 * 8,
        .max_exact_match_keys = 16,
        .ternary_match_xbar = 66 * 8,
        .max_ternary_match_keys = 8,
    };
    break;
  }

  return constraints;
}

TNA::TNA(TNAVersion version)
    : version(version), constraints(constraints_from_version(version)),
      simple_placer(&constraints) {}

TNA::TNA(const TNA &other)
    : version(other.version), constraints(other.constraints),
      simple_placer(other.simple_placer), parser(other.parser) {}

bool TNA::condition_meets_phv_limit(klee::ref<klee::Expr> expr) const {
  kutil::SymbolRetriever retriever;
  retriever.visit(expr);

  const std::vector<klee::ref<klee::ReadExpr>> &chunks =
      retriever.get_retrieved_packet_chunks();

  return static_cast<int>(chunks.size()) <=
         constraints.max_packet_bytes_in_condition;
}

void TNA::place_table(const Table *table,
                      const std::unordered_set<DS_ID> &dependencies) {
  simple_placer.place_table(table, dependencies);
}

PlacementStatus
TNA::can_place_table(const Table *table,
                     const std::unordered_set<DS_ID> &dependencies) const {
  return simple_placer.can_place_table(table, dependencies);
}

void TNA::log_debug_placement() const { simple_placer.log_debug(); }

} // namespace tofino
} // namespace synapse