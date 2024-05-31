#pragma once

#include "data_structures/data_structures.h"
#include "../context.h"

namespace synapse {
namespace x86 {

class x86Context : public TargetContext {
public:
  virtual TargetContext *clone() const override {
    return new x86Context(*this);
  }
};

} // namespace x86
} // namespace synapse