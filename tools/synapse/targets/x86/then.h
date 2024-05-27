#pragma once

#include "x86_module.h"

#include "else.h"

namespace synapse {
namespace x86 {

class Then : public x86Module {
public:
  Then() : x86Module(ModuleType::x86_Then, "Then") {}
  Then(const bdd::Node *node) : x86Module(ModuleType::x86_Then, "Then", node) {}

private:
  generated_data_t process(const EP &ep, const bdd::Node *node) override {
    return generated_data_t();
  }

public:
  virtual void visit(EPVisitor &visitor, const EPNode *ep_node) const override {
    visitor.visit(ep_node, this);
  }

  virtual Module_ptr clone() const override {
    auto cloned = new Then(node);
    return std::shared_ptr<Module>(cloned);
  }

  virtual bool equals(const Module *other) const override {
    return other->get_type() == type;
  }
};

} // namespace x86
} // namespace synapse
