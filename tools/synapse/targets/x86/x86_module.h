#pragma once

#include "../module.h"
#include "../module_generator.h"
#include "data_structures/data_structures.h"
#include "x86_context.h"

namespace synapse {
namespace x86 {

class x86Module : public Module {
public:
  x86Module(ModuleType _type, const std::string &_name, const bdd::Node *node)
      : Module(_type, TargetType::x86, _name, node) {}
};

class x86ModuleGenerator : public ModuleGenerator {
protected:
  ModuleType type;
  TargetType target;

public:
  x86ModuleGenerator(ModuleType _type, const std::string &_name)
      : ModuleGenerator(_type, TargetType::x86, _name) {}
};

} // namespace x86
} // namespace synapse
