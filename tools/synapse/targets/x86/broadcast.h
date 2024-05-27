#pragma once

#include "x86_module.h"

namespace synapse {
namespace x86 {

class Broadcast : public x86Module {
public:
  Broadcast(const bdd::Node *node)
      : x86Module(ModuleType::x86_Broadcast, "Broadcast", node) {}

public:
  virtual void visit(EPVisitor &visitor, const EPNode *ep_node) const override {
    visitor.visit(ep_node, this);
  }

  virtual bool equals(const Module *other) const override {
    return other->get_type() == type;
  }
};

class BroadcastGenerator : public x86ModuleGenerator {
protected:
  ModuleType type;
  TargetType target;

public:
  BroadcastGenerator() : x86ModuleGenerator(ModuleType::x86_Broadcast) {}

protected:
  generated_data_t process_node(const EP &ep, const bdd::Node *node) override {
    generated_data_t result;

    if (node->get_type() != bdd::NodeType::ROUTE) {
      return result;
    }

    const bdd::Route *casted = static_cast<const bdd::Route *>(node);

    if (casted->get_operation() == bdd::RouteOperation::BCAST) {
      std::unique_ptr<Module> new_module = std::make_unique<Broadcast>(node);
      EP new_ep = ep.process_leaf(new_module, node->get_next(), true);

      result.module = new_module.get();
      result.next.push_back(new_ep);
    }

    return result;
  }
};

} // namespace x86
} // namespace synapse
