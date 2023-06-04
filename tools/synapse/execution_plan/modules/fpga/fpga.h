#pragma once

#include "../../target.h"
#include "../module.h"

#include "memory_bank.h"

namespace synapse {
namespace targets {
namespace fpga {

class FPGATarget : public Target {
public:
  FPGATarget()
      : Target(TargetType::FPGA, {},
               TargetMemoryBank_ptr(new FPGAMemoryBank())) {}

  static Target_ptr build() { return Target_ptr(new FPGATarget()); }
};

} // namespace fpga
} // namespace targets
} // namespace synapse
