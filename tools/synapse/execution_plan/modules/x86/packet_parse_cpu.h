#pragma once

#include "../module.h"

namespace synapse {
namespace targets {
namespace x86 {

typedef uint16_t cpu_code_path_t;

class PacketParseCPU : public Module {
private:

public:
  PacketParseCPU()
      : Module(ModuleType::x86_PacketParseCPU, TargetType::x86,
               "PacketParseCPU", nullptr) {}

private:
  processing_result_t process(const ExecutionPlan &ep,
                              BDD::Node_ptr node) override {
    processing_result_t result;
    return result;
  }

public:
  virtual void visit(ExecutionPlanVisitor &visitor,
                     const ExecutionPlanNode *ep_node) const override {
    visitor.visit(ep_node, this);
  }

  virtual Module_ptr clone() const override {
    auto cloned = new PacketParseCPU();
    return std::shared_ptr<Module>(cloned);
  }

  virtual bool equals(const Module *other) const override {
    if (other->get_type() != type) {
      return false;
    }

    return true;
  }
};
} // namespace x86_tofino
} // namespace targets
} // namespace synapse
