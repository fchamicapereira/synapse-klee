#pragma once

#include "x86_module.h"

namespace synapse {
namespace x86 {

class Else : public x86Module {
public:
  Else(const bdd::Node *node) : x86Module(ModuleType::x86_Else, "Else", node) {}

  virtual void visit(EPVisitor &visitor, const EPNode *ep_node) const override {
    visitor.visit(ep_node, this);
  }

  virtual Module *clone() const override {
    Else *cloned = new Else(node);
    return cloned;
  }

  virtual bool equals(const Module *other) const override {
    return other->get_type() == type;
  }
};

class ElseGenerator : public x86ModuleGenerator {
public:
  ElseGenerator() : x86ModuleGenerator(ModuleType::x86_Else) {}

protected:
  virtual modgen_report_t process_node(const EP *ep,
                                       const bdd::Node *node) const override {
    // Never explicitly generate this module from the BDD.
    return modgen_report_t();
  }
};

} // namespace x86
} // namespace synapse
