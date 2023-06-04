#pragma once

#include "../../target.h"
#include "../module.h"

#include "memory_bank.h"

namespace synapse {
namespace targets {
namespace netronome {

class NetronomeTarget : public Target {
public:
  NetronomeTarget()
      : Target(TargetType::Netronome, {},
               TargetMemoryBank_ptr(new NetronomeMemoryBank())) {}

  static Target_ptr build() { return Target_ptr(new NetronomeTarget()); }
};

} // namespace netronome
} // namespace targets
} // namespace synapse
