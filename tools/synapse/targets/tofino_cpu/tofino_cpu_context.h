#pragma once

#include "../context.h"

namespace synapse {
namespace tofino_cpu {

class TofinoCPUContext : public TargetContext {
private:
  uint64_t capacity_pps;

public:
  TofinoCPUContext() : capacity_pps(100'000) {}

  virtual TargetContext *clone() const override {
    return new TofinoCPUContext(*this);
  }

  virtual uint64_t estimate_throughput_pps() const override {
    return capacity_pps;
  }
};

} // namespace tofino_cpu
} // namespace synapse