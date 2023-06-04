#pragma once

#include "../../memory_bank.h"

namespace synapse {
namespace targets {
namespace netronome {

class NetronomeMemoryBank : public TargetMemoryBank {
public:
  virtual TargetMemoryBank_ptr clone() const override {
    return TargetMemoryBank_ptr(new NetronomeMemoryBank());
  }
};

} // namespace netronome
} // namespace targets
} // namespace synapse