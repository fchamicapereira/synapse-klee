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
        .map_ram_per_stage = 128 * 1024 * 48,
        .max_logical_tcam_tables_per_stage = 8,
        .max_logical_sram_and_tcam_tables_per_stage = 16,
        .phv_size = 4000,
        .phv_8bit_containers = 64,
        .phv_16bit_containers = 96,
        .phv_32bit_containers = 64,
        .packet_buffer_size = static_cast<bits_t>(20e6 * 8),
        .exact_match_xbar_per_stage = 128 * 8,
        .max_exact_match_keys = 16,
        .ternary_match_xbar = 66 * 8,
        .max_ternary_match_keys = 8,
        .max_salu_size = 32,
    };
    break;
  case TNAVersion::TNA2:
    constraints = {
        .max_packet_bytes_in_condition = 4,
        .stages = 20,
        .sram_per_stage = 128 * 1024 * 80,
        .tcam_per_stage = 44 * 512 * 24,
        .map_ram_per_stage = 128 * 1024 * 48,
        .max_logical_tcam_tables_per_stage = 8,
        .max_logical_sram_and_tcam_tables_per_stage = 16,
        .phv_size = 5000,
        .phv_8bit_containers = 80,
        .phv_16bit_containers = 120,
        .phv_32bit_containers = 80,
        .packet_buffer_size = static_cast<bits_t>(64e6 * 8),
        .exact_match_xbar_per_stage = 128 * 8,
        .max_exact_match_keys = 16,
        .ternary_match_xbar = 66 * 8,
        .max_ternary_match_keys = 8,
        .max_salu_size = 32, // TNA2 actually can supposedly support 64b, but it
                             // keeps crashing the compiler...
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

void TNA::place(const DS *ds, const std::unordered_set<DS_ID> &deps) {
  simple_placer.place(ds, deps);
}

void TNA::place_many(const std::vector<std::unordered_set<DS *>> &all_ds,
                     const std::unordered_set<DS_ID> &_deps) {
  std::unordered_set<DS_ID> deps = _deps;

  for (const std::unordered_set<DS *> &indep_ds : all_ds) {
    std::unordered_set<DS_ID> new_deps;

    for (const DS *ds : indep_ds) {
      place(ds, deps);
      new_deps.insert(ds->id);
    }

    deps.insert(new_deps.begin(), new_deps.end());
  }
}

PlacementStatus TNA::can_place(const DS *ds,
                               const std::unordered_set<DS_ID> &deps) const {
  return simple_placer.can_place(ds, deps);
}

PlacementStatus
TNA::can_place_many(const std::vector<std::unordered_set<DS *>> &candidates,
                    const std::unordered_set<DS_ID> &_deps) const {
  SimplePlacer snapshot(simple_placer);
  std::unordered_set<DS_ID> deps = _deps;

  for (const std::unordered_set<DS *> &indep_ds : candidates) {
    std::unordered_set<DS_ID> new_deps;

    for (const DS *ds : indep_ds) {
      PlacementStatus status = snapshot.can_place(ds, deps);

      if (status != PlacementStatus::SUCCESS) {
        return status;
      }

      snapshot.place(ds, deps);
      new_deps.insert(ds->id);
    }

    deps.insert(new_deps.begin(), new_deps.end());
  }

  return PlacementStatus::SUCCESS;
}

void TNA::log_debug_placement() const { simple_placer.log_debug(); }

} // namespace tofino
} // namespace synapse