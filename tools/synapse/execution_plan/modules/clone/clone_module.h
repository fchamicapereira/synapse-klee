#pragma once

#include "../module.h"
#include "memory_bank.h"

namespace synapse {
namespace targets {
namespace clone {

class CloneModule : public Module {
public:
  CloneModule(ModuleType _type, const char *_name)
    : Module(_type, TargetType::CloNe, _name) {}
  
  CloneModule(ModuleType _type, const char *_name, BDD::Node_ptr node)
    : Module(_type, TargetType::CloNe, _name, node) {}

  virtual void visit(ExecutionPlanVisitor &visitor, const ExecutionPlanNode *ep_node) const = 0;
  virtual Module_ptr clone() const = 0;
  virtual bool equals(const Module *other) const = 0;
};

} // namespace clone
} // namespace targets
} // namespace synapse