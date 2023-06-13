#pragma once

#include <memory>
#include <vector>
#include <unordered_map>
#include <string>

#include "instance.h"

namespace synapse {

class Module;
typedef std::shared_ptr<Module> Module_ptr;

class TargetMemoryBank;
typedef std::shared_ptr<TargetMemoryBank> TargetMemoryBank_ptr;

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

extern std::unordered_map<std::string, TargetType> string_to_target_type;

typedef uint64_t target_id_t;

std::ostream &operator<<(std::ostream &os, TargetType type);

struct Target;

typedef std::shared_ptr<Target> Target_ptr;

struct Target {
private:
  static target_id_t id_counter;
public:
  const TargetType type;
  const std::vector<Module_ptr> modules;
  const TargetMemoryBank_ptr memory_bank;
  const Instance_ptr instance;
  const target_id_t id;


  Target(TargetType _type, const std::vector<Module_ptr> &_modules,
         const TargetMemoryBank_ptr &_memory_bank);
  
  Target(TargetType _type, const std::vector<Module_ptr> &_modules,
         const TargetMemoryBank_ptr &_memory_bank, Instance_ptr _instance);

  Target_ptr clone() const;

  
};


} // namespace synapse