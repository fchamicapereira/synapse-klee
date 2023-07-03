#pragma once

#include "../../target.h"
#include "../module.h"

#include "memory_bank.h"

namespace synapse {
namespace targets {
namespace fpga {

class FPGATarget : public Target {
public:
  FPGATarget(Instance_ptr _instance)
      : Target(TargetType::FPGA, {},
               TargetMemoryBank_ptr(new FPGAMemoryBank()),
               _instance) {}

  static Target_ptr build(Instance_ptr _instance = nullptr) { return Target_ptr(new FPGATarget(_instance)); }
};

} // namespace fpga
} // namespace targets
} // namespace synapse
