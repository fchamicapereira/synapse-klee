#pragma once

#include "../context.h"

namespace synapse {
namespace tofino_cpu {

class TofinoCPUContext : public TargetContext {
public:
  virtual TargetContext *clone() const override {
    return new TofinoCPUContext(*this);
  }
};

} // namespace tofino_cpu
} // namespace synapse