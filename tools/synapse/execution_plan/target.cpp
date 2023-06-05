#include "target.h"
#include "memory_bank.h"
#include "modules/module.h"

namespace synapse {

std::unordered_map<std::string, TargetType> string_to_target_type = {
    {"x86", TargetType::x86},
    {"x86_bmv2", TargetType::x86_BMv2},
    {"x86_tofino", TargetType::x86_Tofino},
    {"tofino", TargetType::Tofino},
    {"netronome", TargetType::Netronome},
    {"fpga", TargetType::FPGA},
    {"bmv2", TargetType::BMv2},
    {"clone", TargetType::CloNe},
};

Target::Target(TargetType _type, const std::vector<Module_ptr> &_modules,
               const TargetMemoryBank_ptr &_memory_bank)
    : type(_type), modules(_modules), memory_bank(_memory_bank) {}

std::ostream &operator<<(std::ostream &os, TargetType type) {
  os << Module::target_to_string(type);
  return os;
}

} // namespace synapse