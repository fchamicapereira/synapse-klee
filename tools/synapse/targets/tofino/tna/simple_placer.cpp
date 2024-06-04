#include "simple_placer.h"
#include "tna.h"

#include "../../../log.h"

namespace synapse {
namespace tofino {

std::ostream &operator<<(std::ostream &os, const PlacementStatus &status) {
  switch (status) {
  case PlacementStatus::SUCCESS:
    os << "SUCCESS";
    break;
  case PlacementStatus::TABLE_TOO_LARGE:
    os << "TABLE_TOO_LARGE";
    break;
  case PlacementStatus::NO_AVAILABLE_TABLES:
    os << "NO_AVAILABLE_TABLES";
    break;
  case PlacementStatus::TOO_MANY_KEYS:
    os << "TOO_MANY_KEYS";
    break;
  case PlacementStatus::XBAR_CONSUME_EXCEEDS_LIMIT:
    os << "XBAR_CONSUME_EXCEEDS_LIMIT";
    break;
  case PlacementStatus::NO_AVAILABLE_STAGE:
    os << "NO_AVAILABLE_STAGE";
    break;
  }
  return os;
}

static std::vector<Stage> create_stages(const TNAConstraints *constraints) {
  std::vector<Stage> stages;

  for (int stage_id = 0; stage_id < constraints->stages; stage_id++) {
    Stage s = {
        .stage_id = stage_id,
        .available_sram = constraints->sram_per_stage,
        .available_tcam = constraints->tcam_per_stage,
        .tables = {},
    };

    stages.push_back(s);
  }

  return stages;
}

SimplePlacer::SimplePlacer(const TNAConstraints *_constraints)
    : constraints(_constraints), stages(create_stages(_constraints)),
      total_tables(0) {}

SimplePlacer::SimplePlacer(const SimplePlacer &other)
    : constraints(other.constraints), stages(other.stages),
      total_tables(other.total_tables) {}

static int
get_soonest_available_stage(const std::vector<Stage> &stages,
                            const std::unordered_set<DS_ID> &dependencies) {
  const Stage *soonest_stage = nullptr;

  for (auto it = stages.rbegin(); it != stages.rend(); it++) {
    const Stage *stage = &(*it);

    bool can_place = true;
    for (DS_ID dependency : dependencies) {
      if (stage->tables.find(dependency) != stage->tables.end()) {
        can_place = false;
        break;
      }
    }

    if (can_place) {
      soonest_stage = stage;
    }
  }

  if (soonest_stage) {
    return soonest_stage->stage_id;
  }

  return -1;
}

struct SimplePlacer::table_placement_t {
  int stage_id;
  bits_t sram;
};

PlacementStatus SimplePlacer::find_placements(
    const Table *table, const std::unordered_set<DS_ID> &dependencies,
    std::vector<table_placement_t> &placements) const {
  if (total_tables == constraints->max_logical_sram_and_tcam_tables) {
    return PlacementStatus::NO_AVAILABLE_TABLES;
  }

  if (static_cast<int>(table->keys.size()) >
      constraints->max_exact_match_keys) {
    return PlacementStatus::TOO_MANY_KEYS;
  }

  if (table->get_match_xbar_consume() > constraints->exact_match_xbar) {
    return PlacementStatus::XBAR_CONSUME_EXCEEDS_LIMIT;
  }

  int soonest_stage_id = get_soonest_available_stage(stages, dependencies);
  assert(soonest_stage_id < static_cast<int>(stages.size()));

  if (soonest_stage_id < 0) {
    return PlacementStatus::NO_AVAILABLE_STAGE;
  }

  bits_t requested_sram = table->get_consumed_sram();
  int total_stages = stages.size();

  for (int stage_id = soonest_stage_id; stage_id < total_stages; stage_id++) {
    const Stage *stage = &stages[stage_id];

    if (stage->available_sram == 0) {
      continue;
    }

    // This is not actually how it happens, but this is a VERY simple placer.
    bits_t amount_placed = std::min(requested_sram, stage->available_sram);
    requested_sram -= amount_placed;
    placements.push_back({stage_id, amount_placed});

    if (requested_sram == 0) {
      break;
    }
  }

  if (requested_sram > 0) {
    return PlacementStatus::TABLE_TOO_LARGE;
  }

  return PlacementStatus::SUCCESS;
}

void SimplePlacer::place_table(const Table *table,
                               const std::unordered_set<DS_ID> &dependencies) {
  std::vector<table_placement_t> placements;

  PlacementStatus status = find_placements(table, dependencies, placements);
  assert(status == PlacementStatus::SUCCESS && "Cannot place table");

  for (const table_placement_t &placement : placements) {
    assert(placement.stage_id < static_cast<int>(stages.size()));
    Stage *stage = &stages[placement.stage_id];

    stage->available_sram -= placement.sram;
    stage->tables.insert(table->id);
  }

  table->log_debug();
  log_debug();
}

PlacementStatus SimplePlacer::can_place_table(
    const Table *table, const std::unordered_set<DS_ID> &dependencies) const {
  std::vector<table_placement_t> placements;
  return find_placements(table, dependencies, placements);
}

void SimplePlacer::log_debug() const {
  Log::dbg() << "\n";
  Log::dbg() << "====================== SimplePlacer ======================\n";

  for (const Stage &stage : stages) {
    if (stage.tables.empty()) {
      continue;
    }

    std::stringstream ss;

    float sram_usage =
        100.0 - (stage.available_sram * 100.0) / constraints->sram_per_stage;
    float tcam_usage =
        100.0 - (stage.available_tcam * 100.0) / constraints->tcam_per_stage;

    ss << "Stage " << stage.stage_id << ": ";
    ss << "SRAM=" << stage.available_sram / 8 << "B ";
    ss << "(" << sram_usage << "%) ";
    ss << "TCAM=" << stage.available_tcam / 8 << "B ";
    ss << "(" << tcam_usage << "%) ";
    ss << "Tables=[";
    bool first = true;
    for (DS_ID table_id : stage.tables) {
      if (!first) {
        ss << ",";
      }
      ss << table_id;
      first = false;
    }
    ss << "]\n";

    Log::dbg() << ss.str();
  }

  Log::dbg() << "==========================================================\n";
}

} // namespace tofino
} // namespace synapse
