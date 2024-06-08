#pragma once

#include "../data_structures/data_structures.h"

#include <unordered_set>

namespace synapse {
namespace tofino {

enum class PlacementStatus {
  SUCCESS,
  TOO_LARGE,
  TOO_MANY_KEYS,
  XBAR_CONSUME_EXCEEDS_LIMIT,
  NO_AVAILABLE_STAGE,
  UNKNOWN,
};

std::ostream &operator<<(std::ostream &os, const PlacementStatus &status);

struct Stage {
  int stage_id;
  bits_t available_sram;
  bits_t available_tcam;
  bits_t available_map_ram;
  bits_t available_exact_match_xbar;
  int available_logical_ids;
  std::unordered_set<DS_ID> tables;
};

struct TNAConstraints;

class SimplePlacer {
private:
  const TNAConstraints *constraints;
  std::vector<Stage> stages;

public:
  SimplePlacer(const TNAConstraints *constraints);
  SimplePlacer(const SimplePlacer &other);

  void place(const DS *ds, const std::unordered_set<DS_ID> &deps);
  PlacementStatus can_place(const DS *ds,
                            const std::unordered_set<DS_ID> &deps) const;

  void log_debug() const;

private:
  struct placement_t;

  void place(const Table *table, const std::unordered_set<DS_ID> &deps);
  void place(const Register *reg, const std::unordered_set<DS_ID> &deps);
  void place(const CachedTable *cached_table,
             const std::unordered_set<DS_ID> &deps);

  PlacementStatus can_place(const Table *table,
                            const std::unordered_set<DS_ID> &deps) const;
  PlacementStatus can_place(const Register *reg,
                            const std::unordered_set<DS_ID> &deps) const;
  PlacementStatus can_place(const CachedTable *cached_table,
                            const std::unordered_set<DS_ID> &deps) const;

  PlacementStatus find_placements(const Table *table,
                                  const std::unordered_set<DS_ID> &deps,
                                  std::vector<placement_t> &placements) const;
  PlacementStatus find_placements(const Register *reg,
                                  const std::unordered_set<DS_ID> &deps,
                                  std::vector<placement_t> &placements) const;

  void concretize_placement(Stage *stage, const placement_t &placement);
};

} // namespace tofino
} // namespace synapse