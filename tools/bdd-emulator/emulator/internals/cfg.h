#pragma once

#include "call-paths-to-bdd.h"
#include "klee-util.h"

#include "base-types.h"
#include "byte.h"

#include <assert.h>
#include <unordered_map>

namespace BDD {
namespace emulation {

struct cfg_t {
  std::pair<bool, gbps_t> rate;
  std::pair<bool, time_us_t> timeout;
  int loops;
  bool warmup;
  bool report;

  cfg_t() : loops(1), warmup(false), report(false) {}
};

} // namespace emulation
} // namespace BDD