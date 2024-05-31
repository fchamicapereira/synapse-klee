#pragma once

#include "../context.h"
#include "tna_arch/tna_arch.h"

namespace synapse {
namespace tofino {

class TofinoContext : public TargetContext {
private:
  TNA tna;

public:
  TofinoContext(TNAVersion version) : tna(version) {}
  TofinoContext(const TofinoContext &other) : tna(other.tna) {}

  virtual TargetContext *clone() const override {
    return new TofinoContext(*this);
  }

  TNA &get_mutable_tna() { return tna; }
  const TNA &get_tna() const { return tna; }
};

} // namespace tofino
} // namespace synapse