#pragma once

#include "../../memory_bank.h"

namespace synapse {
namespace targets {
namespace x86_bmv2 {

class x86BMv2MemoryBank : public TargetMemoryBank {
public:
  virtual TargetMemoryBank_ptr clone() const override {
    return TargetMemoryBank_ptr(new x86BMv2MemoryBank());
  }
};

} // namespace x86_bmv2
} // namespace targets
} // namespace synapse