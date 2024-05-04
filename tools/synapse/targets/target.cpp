#include "target.h"
#include "memory_bank.h"
#include "modules/module.h"

namespace synapse {

Target::Target(TargetType _type, const std::vector<Module_ptr> &_modules,
               const TargetMemoryBank_ptr &_memory_bank)
    : type(_type), modules(_modules), memory_bank(_memory_bank) {}

std::ostream &operator<<(std::ostream &os, TargetType type) {
  os << Module::target_to_string(type);
  return os;
}

} // namespace synapse