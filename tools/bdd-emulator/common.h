#pragma once

#include "emulator/internals/internals.h"

#include <unordered_map>

namespace BDD {

typedef std::unordered_map<node_id_t, emulation::hit_rate_t> bdd_hit_rate_t;

} // namespace BDD