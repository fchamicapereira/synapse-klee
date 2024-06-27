#pragma once

#include "data_structures/data_structures.h"
#include "../context.h"

namespace synapse {
namespace x86 {

class x86Context : public TargetContext {
private:
  uint64_t capacity_pps;

public:
  x86Context() : capacity_pps(120'000'000) {}

  virtual TargetContext *clone() const override {
    return new x86Context(*this);
  }

  virtual uint64_t estimate_throughput_pps() const override {
    return capacity_pps;
  }
};

} // namespace x86
} // namespace synapse