#pragma once

#include "../../memory_bank.h"

namespace synapse {
namespace targets {
namespace bmv2 {

class BMv2MemoryBank : public TargetMemoryBank {
public:
  virtual TargetMemoryBank_ptr clone() const override {
    return TargetMemoryBank_ptr(new BMv2MemoryBank());
  }
};

} // namespace bmv2
} // namespace targets
} // namespace synapse