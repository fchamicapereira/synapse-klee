#pragma once

#include "x86_module.h"

namespace synapse {
namespace x86 {

class Then : public x86Module {
public:
  Then(const bdd::Node *node) : x86Module(ModuleType::x86_Then, "Then", node) {}

  virtual void visit(EPVisitor &visitor, const EPNode *ep_node) const override {
    visitor.visit(ep_node, this);
  }

  virtual Module *clone() const override {
    Then *cloned = new Then(node);
    return cloned;
  }

  virtual bool equals(const Module *other) const override {
    return other->get_type() == type;
  }
};

class ThenGenerator : public x86ModuleGenerator {
public:
  ThenGenerator() : x86ModuleGenerator(ModuleType::x86_Then) {}

protected:
  virtual modgen_report_t process_node(const EP *ep,
                                       const bdd::Node *node) const override {
    // Never explicitly generate this module from the BDD.
    return modgen_report_t();
  }
};

} // namespace x86
} // namespace synapse
