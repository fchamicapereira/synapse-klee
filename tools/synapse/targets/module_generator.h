#pragma once

#include "call-paths-to-bdd.h"

#include "target.h"
#include "module.h"
#include "../execution_plan/execution_plan.h"

namespace synapse {

struct modgen_report_t {
  const Module *module;
  std::vector<EP> next;

  modgen_report_t() : module(nullptr) {}
};

class ModuleGenerator {
protected:
  ModuleType type;
  TargetType target;

public:
  ModuleGenerator(ModuleType _type, TargetType _target)
      : type(_type), target(_target) {}

  virtual ~ModuleGenerator() {}

  modgen_report_t generate(const EP &ep, const bdd::Node *node) const;

protected:
  virtual modgen_report_t process_node(const EP &ep,
                                       const bdd::Node *node) const = 0;
};

} // namespace synapse