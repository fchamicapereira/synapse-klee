#pragma once

#include "../../target.h"
#include "../module.h"

#include "memory_bank.h"

namespace synapse {
namespace targets {
namespace netronome {

class NetronomeTarget : public Target {
public:
  NetronomeTarget(Instance_ptr _instance)
      : Target(TargetType::Netronome, {},
               TargetMemoryBank_ptr(new NetronomeMemoryBank()),
               _instance) {}

  static Target_ptr build(Instance_ptr _instance = nullptr) { return Target_ptr(new NetronomeTarget(_instance)); }
};

} // namespace netronome
} // namespace targets
} // namespace synapse
