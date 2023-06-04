#pragma once

#include "../../memory_bank.h"

namespace synapse {
namespace targets {
namespace fpga {

class FPGAMemoryBank : public TargetMemoryBank {
public:
  virtual TargetMemoryBank_ptr clone() const {
    return TargetMemoryBank_ptr(new FPGAMemoryBank());
  }
};

} // namespace fpga
} // namespace targets
} // namespace synapse