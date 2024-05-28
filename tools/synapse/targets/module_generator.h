#pragma once

#include "call-paths-to-bdd.h"

#include "target.h"
#include "module.h"
#include "../execution_plan/execution_plan.h"

namespace synapse {

struct modgen_report_t {
  bool success;
  const Module *module;
  std::vector<const EP *> next;

  modgen_report_t() : success(false), module(nullptr) {}

  modgen_report_t(const Module *_module, const std::vector<const EP *> &_next)
      : success(true), module(_module), next(_next) {
    assert(module && "Module generator should return a module");
    assert(next.size() > 0 &&
           "Module generator should return at least one execution plan");
  }
};

class ModuleGenerator {
protected:
  ModuleType type;
  TargetType target;

public:
  ModuleGenerator(ModuleType _type, TargetType _target)
      : type(_type), target(_target) {}

  virtual ~ModuleGenerator() {}

  modgen_report_t generate(const EP *ep, const bdd::Node *node,
                           bool reorder_bdd) const;

protected:
  virtual modgen_report_t process_node(const EP *ep,
                                       const bdd::Node *node) const = 0;
};

} // namespace synapse