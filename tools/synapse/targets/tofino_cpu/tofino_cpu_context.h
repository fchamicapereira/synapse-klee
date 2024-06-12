#pragma once

#include "../context.h"

namespace synapse {
namespace tofino_cpu {

class TofinoCPUContext : public TargetContext {
private:
  int capacity_kpps;

public:
  TofinoCPUContext() : capacity_kpps(100) {}

  virtual TargetContext *clone() const override {
    return new TofinoCPUContext(*this);
  }

  virtual int estimate_throughput_kpps() const override {
    return capacity_kpps;
  }
};

} // namespace tofino_cpu
} // namespace synapse