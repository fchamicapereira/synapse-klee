#pragma once

#include "emulator/internals/internals.h"

#include <unordered_map>

namespace bdd {

typedef std::unordered_map<node_id_t, emulation::hit_rate_t> bdd_hit_rate_t;

} // namespace bdd
