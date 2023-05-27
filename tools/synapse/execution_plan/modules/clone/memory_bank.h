#pragma once

#include <klee/Expr.h>

#include <string>
#include <unordered_map>
#include <unordered_set>

#include "../../../symbex.h"


namespace synapse {
namespace targets {
namespace clone {

class CloneMemoryBank : public MemoryBank {
public:
  CloneMemoryBank() : MemoryBank() {}

  CloneMemoryBank(const MemoryBank &mb) : MemoryBank(mb) {}

  CloneMemoryBank(const CloneMemoryBank &mb) : MemoryBank(mb) {}

  virtual MemoryBank_ptr clone() const {
    auto clone = new CloneMemoryBank(*this);
    return MemoryBank_ptr(clone);
  }
};

} // namespace clone
} // namespace targets
} // namespace synapse