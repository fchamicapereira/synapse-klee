#pragma once

#include "../data_structures/data_structures.h"

#include <unordered_set>

namespace synapse {
namespace tofino {

enum class PlacementStatus {
  SUCCESS,
  TOO_LARGE,
  NO_LOGICAL_TABLES_AVAILABLE,
  TOO_MANY_KEYS,
  XBAR_CONSUME_EXCEEDS_LIMIT,
  NO_AVAILABLE_STAGE,
};

std::ostream &operator<<(std::ostream &os, const PlacementStatus &status);

struct Stage {
  int stage_id;
  bits_t available_sram;
  bits_t available_tcam;
  bits_t available_exact_match_xbar;
  std::unordered_set<DS_ID> tables;
};

struct TNAConstraints;

class SimplePlacer {
private:
  const TNAConstraints *constraints;
  std::vector<Stage> stages;
  int total_logical_tables;

public:
  SimplePlacer(const TNAConstraints *constraints);
  SimplePlacer(const SimplePlacer &other);

  void place_table(const Table *table, const std::unordered_set<DS_ID> &deps);
  void place_register(const Register *reg,
                      const std::unordered_set<DS_ID> &deps);

  PlacementStatus can_place_table(const Table *table,
                                  const std::unordered_set<DS_ID> &deps) const;
  PlacementStatus
  can_place_register(const Register *reg,
                     const std::unordered_set<DS_ID> &deps) const;

  void log_debug() const;

private:
  struct table_placement_t;

  PlacementStatus
  find_placements(const Table *table, const std::unordered_set<DS_ID> &deps,
                  std::vector<table_placement_t> &placements) const;
  PlacementStatus
  find_placements(const Register *reg, const std::unordered_set<DS_ID> &deps,
                  std::vector<table_placement_t> &placements) const;
};

} // namespace tofino
} // namespace synapse