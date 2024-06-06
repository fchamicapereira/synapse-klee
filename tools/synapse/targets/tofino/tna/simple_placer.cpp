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
  case PlacementStatus::TOO_LARGE:
    os << "TOO_LARGE";
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
  case PlacementStatus::UNKNOWN:
    os << "UNKNOWN";
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
        .available_map_ram = constraints->map_ram_per_stage,
        .available_exact_match_xbar = constraints->exact_match_xbar_per_stage,
        .available_logical_ids =
            constraints->max_logical_sram_and_tcam_tables_per_stage,
        .tables = {},
    };

    stages.push_back(s);
  }

  return stages;
}

SimplePlacer::SimplePlacer(const TNAConstraints *_constraints)
    : constraints(_constraints), stages(create_stages(_constraints)) {}

SimplePlacer::SimplePlacer(const SimplePlacer &other)
    : constraints(other.constraints), stages(other.stages) {}

static int get_soonest_available_stage(const std::vector<Stage> &stages,
                                       const std::unordered_set<DS_ID> &deps) {
  const Stage *soonest_stage = nullptr;

  for (auto it = stages.rbegin(); it != stages.rend(); it++) {
    const Stage *stage = &(*it);

    bool can_place = true;
    for (DS_ID dependency : deps) {
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

struct SimplePlacer::placement_t {
  int stage_id;
  bits_t sram;
  bits_t map_ram;
  bits_t xbar;
  int logical_ids;
  DS_ID obj;
};

void SimplePlacer::concretize_placement(
    Stage *stage, const SimplePlacer::placement_t &placement) {
  assert(stage->stage_id == placement.stage_id);

  assert(stage->available_sram >= placement.sram);
  assert(stage->available_map_ram >= placement.map_ram);
  assert(stage->available_exact_match_xbar >= placement.xbar);
  assert(stage->available_logical_ids >= placement.logical_ids);

  stage->available_sram -= placement.sram;
  stage->available_map_ram -= placement.map_ram;
  stage->available_exact_match_xbar -= placement.xbar;
  stage->available_logical_ids -= placement.logical_ids;
  stage->tables.insert(placement.obj);
}

PlacementStatus
SimplePlacer::find_placements(const Table *table,
                              const std::unordered_set<DS_ID> &deps,
                              std::vector<placement_t> &placements) const {
  if (static_cast<int>(table->keys.size()) >
      constraints->max_exact_match_keys) {
    return PlacementStatus::TOO_MANY_KEYS;
  }

  if (table->get_match_xbar_consume() >
      constraints->exact_match_xbar_per_stage) {
    return PlacementStatus::XBAR_CONSUME_EXCEEDS_LIMIT;
  }

  int soonest_stage_id = get_soonest_available_stage(stages, deps);
  assert(soonest_stage_id < static_cast<int>(stages.size()));

  if (soonest_stage_id < 0) {
    return PlacementStatus::NO_AVAILABLE_STAGE;
  }

  bits_t requested_sram = table->get_consumed_sram();
  bits_t requested_xbar = table->get_match_xbar_consume();

  int total_stages = stages.size();
  for (int stage_id = soonest_stage_id; stage_id < total_stages; stage_id++) {
    const Stage *stage = &stages[stage_id];

    if (stage->available_sram == 0) {
      continue;
    }

    if (requested_xbar > stage->available_exact_match_xbar) {
      continue;
    }

    if (stage->available_logical_ids == 0) {
      continue;
    }

    // This is not actually how it happens, but this is a VERY simple placer.
    bits_t amount_placed = std::min(requested_sram, stage->available_sram);

    placement_t placement = {
        .stage_id = stage_id,
        .sram = amount_placed,
        .map_ram = 0,
        .xbar = requested_xbar,
        .logical_ids = 1,
        .obj = table->id,
    };

    requested_sram -= amount_placed;
    placements.push_back(placement);

    if (requested_sram == 0) {
      break;
    }
  }

  if (requested_sram > 0) {
    return PlacementStatus::TOO_LARGE;
  }

  return PlacementStatus::SUCCESS;
}

PlacementStatus
SimplePlacer::find_placements(const Register *reg,
                              const std::unordered_set<DS_ID> &deps,
                              std::vector<placement_t> &placements) const {
  int soonest_stage_id = get_soonest_available_stage(stages, deps);
  assert(soonest_stage_id < static_cast<int>(stages.size()));

  if (soonest_stage_id < 0) {
    return PlacementStatus::NO_AVAILABLE_STAGE;
  }

  bits_t requested_sram = reg->get_consumed_sram();
  bits_t requested_map_ram = requested_sram;
  bits_t requested_xbar = reg->index_size;

  int total_stages = stages.size();
  for (int stage_id = soonest_stage_id; stage_id < total_stages; stage_id++) {
    const Stage *stage = &stages[stage_id];

    // Note that for tables we can span them across multiple stages, but we
    // can't do that with registers.
    if (requested_sram > stage->available_sram) {
      continue;
    }

    if (requested_map_ram > stage->available_map_ram) {
      continue;
    }

    if (requested_xbar > stage->available_exact_match_xbar) {
      continue;
    }

    if (stage->available_logical_ids < reg->num_actions) {
      continue;
    }

    placement_t placement = {
        .stage_id = stage_id,
        .sram = requested_sram,
        .map_ram = requested_map_ram,
        .xbar = requested_xbar,
        .logical_ids = reg->num_actions,
        .obj = reg->id,
    };

    requested_sram = 0;
    placements.push_back(placement);

    break;
  }

  if (requested_sram > 0) {
    return PlacementStatus::TOO_LARGE;
  }

  return PlacementStatus::SUCCESS;
}

void SimplePlacer::place(const Table *table,
                         const std::unordered_set<DS_ID> &deps) {
  std::vector<placement_t> placements;

  PlacementStatus status = find_placements(table, deps, placements);
  assert(status == PlacementStatus::SUCCESS && "Cannot place table");

  for (const placement_t &placement : placements) {
    assert(placement.stage_id < static_cast<int>(stages.size()));
    Stage *stage = &stages[placement.stage_id];
    concretize_placement(stage, placement);
  }

  table->log_debug();
  log_debug();
}

void SimplePlacer::place(const Register *reg,
                         const std::unordered_set<DS_ID> &deps) {
  std::vector<placement_t> placements;

  PlacementStatus status = find_placements(reg, deps, placements);
  assert(status == PlacementStatus::SUCCESS && "Cannot place register");

  for (const placement_t &placement : placements) {
    assert(placement.stage_id < static_cast<int>(stages.size()));
    Stage *stage = &stages[placement.stage_id];
    concretize_placement(stage, placement);
  }

  reg->log_debug();
  log_debug();
}

void SimplePlacer::place(const DS *ds, const std::unordered_set<DS_ID> &deps) {
  switch (ds->type) {
  case DSType::TABLE:
    place(static_cast<const Table *>(ds), deps);
    break;
  case DSType::REGISTER:
    place(static_cast<const Register *>(ds), deps);
    break;
  }

  // std::cerr << "PLACEMENT!\n";
  // DEBUG_PAUSE
}

PlacementStatus
SimplePlacer::can_place(const Table *table,
                        const std::unordered_set<DS_ID> &deps) const {
  std::vector<placement_t> placements;
  return find_placements(table, deps, placements);
}

PlacementStatus
SimplePlacer::can_place(const Register *reg,
                        const std::unordered_set<DS_ID> &deps) const {
  std::vector<placement_t> placements;
  return find_placements(reg, deps, placements);
}

PlacementStatus
SimplePlacer::can_place(const DS *ds,
                        const std::unordered_set<DS_ID> &deps) const {
  PlacementStatus status = PlacementStatus::UNKNOWN;

  switch (ds->type) {
  case DSType::TABLE:
    status = can_place(static_cast<const Table *>(ds), deps);
    break;
  case DSType::REGISTER:
    status = can_place(static_cast<const Register *>(ds), deps);
    break;
  }

  return status;
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
    bits_t sram_consumed = constraints->sram_per_stage - stage.available_sram;

    float tcam_usage =
        100.0 - (stage.available_tcam * 100.0) / constraints->tcam_per_stage;
    bits_t tcam_consumed = constraints->tcam_per_stage - stage.available_tcam;

    float map_ram_usage = 100.0 - (stage.available_map_ram * 100.0) /
                                      constraints->map_ram_per_stage;
    bits_t map_ram_consumed =
        constraints->map_ram_per_stage - stage.available_map_ram;

    float xbar_usage = 100.0 - (stage.available_exact_match_xbar * 100.0) /
                                   constraints->exact_match_xbar_per_stage;
    bits_t xbar_consumed = constraints->exact_match_xbar_per_stage -
                           stage.available_exact_match_xbar;

    ss << "Stage " << stage.stage_id << ": ";

    ss << "SRAM=";
    ss << sram_consumed / 8;
    ss << "/";
    ss << constraints->sram_per_stage / 8;
    ss << " B ";
    ss << "(" << sram_usage << "%) ";

    ss << "TCAM=";
    ss << tcam_consumed / 8;
    ss << "/";
    ss << constraints->tcam_per_stage / 8;
    ss << " B ";
    ss << "(" << tcam_usage << "%) ";

    ss << "MapRAM=";
    ss << map_ram_consumed / 8;
    ss << "/";
    ss << constraints->map_ram_per_stage / 8;
    ss << " B ";
    ss << "(" << map_ram_usage << "%) ";

    ss << "ExactMatchXBar=";
    ss << xbar_consumed / 8;
    ss << "/";
    ss << constraints->exact_match_xbar_per_stage / 8;
    ss << " B ";
    ss << "(" << xbar_usage << "%) ";

    ss << "Objs=[";
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
