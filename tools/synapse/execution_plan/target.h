#pragma once

#include <memory>
#include <vector>

namespace synapse {

class Module;
typedef std::shared_ptr<Module> Module_ptr;

class MemoryBank;
typedef std::shared_ptr<MemoryBank> MemoryBank_ptr;

enum TargetType {
  x86,
  x86_BMv2,
  x86_Tofino,
  Tofino,
  Netronome,
  FPGA,
  BMv2,
  CloNe,
};

std::ostream &operator<<(std::ostream &os, TargetType type);

struct Target {
  const TargetType type;
  const std::vector<Module_ptr> modules;
  const MemoryBank_ptr memory_bank;

  Target(TargetType _type, const std::vector<Module_ptr> &_modules,
         const MemoryBank_ptr &_memory_bank);
};

typedef std::shared_ptr<Target> Target_ptr;

} // namespace synapse