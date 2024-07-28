#pragma once

#include "target.h"
#include "context.h"

#include "x86/x86.h"
#include "tofino/tofino.h"
#include "tofino_cpu/tofino_cpu.h"

#include "../profiler.h"

namespace synapse {

inline targets_t build_targets(const Profiler *profiler) {
  return {
      new tofino::TofinoTarget(tofino::TNAVersion::TNA2, profiler),
      new tofino_cpu::TofinoCPUTarget(),
      new x86::x86Target(),
  };
}

inline void delete_targets(targets_t &targets) {
  for (const Target *target : targets) {
    delete target;
  }
  targets.clear();
}

} // namespace synapse