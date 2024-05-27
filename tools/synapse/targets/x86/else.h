#pragma once

#include "x86_module.h"

namespace synapse {
namespace x86 {

class Else : public x86Module {
public:
  Else() : x86Module(ModuleType::x86_Else, "Else") {}
  Else(const bdd::Node *node) : x86Module(ModuleType::x86_Else, "Else", node) {}

private:
  generated_data_t process(const EP &ep, const bdd::Node *node) override {
    return generated_data_t();
  }

public:
  virtual void visit(EPVisitor &visitor, const EPNode *ep_node) const override {
    visitor.visit(ep_node, this);
  }

  virtual Module_ptr clone() const override {
    auto cloned = new Else(node);
    return std::shared_ptr<Module>(cloned);
  }

  virtual bool equals(const Module *other) const override {
    return other->get_type() == type;
  }
};

} // namespace x86
} // namespace synapse
