#pragma once

#include "data_structures/data_structures.h"
#include "../context.h"

namespace synapse {
namespace x86 {

class x86Context : public TargetContext {
private:
  int capacity_kpps;

public:
  x86Context() : capacity_kpps(120'000) {}

  virtual TargetContext *clone() const override {
    return new x86Context(*this);
  }

  virtual int estimate_throughput_kpps() const override {
    return capacity_kpps;
  }
};

} // namespace x86
} // namespace synapse