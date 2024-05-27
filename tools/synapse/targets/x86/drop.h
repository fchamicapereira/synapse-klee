#pragma once

#include "x86_module.h"

namespace synapse {
namespace x86 {

class Drop : public x86Module {
public:
  Drop() : x86Module(ModuleType::x86_Drop, "Drop") {}
  Drop(const bdd::Node *node) : x86Module(ModuleType::x86_Drop, "Drop", node) {}

private:
  generated_data_t process(const EP &ep, const bdd::Node *node) override {
    generated_data_t result;

    auto casted = static_cast<const bdd::Route *>(node);

    if (!casted) {
      return result;
    }

    if (casted->get_operation() == bdd::RouteOperation::DROP) {
      auto new_module = std::make_shared<Drop>(node);
      auto new_ep = ep.process_leaf(new_module, node->get_next(), true);

      result.module = new_module;
      result.next_eps.push_back(new_ep);
    }

    return result;
  }

public:
  virtual void visit(EPVisitor &visitor, const EPNode *ep_node) const override {
    visitor.visit(ep_node, this);
  }

  virtual Module_ptr clone() const override {
    auto cloned = new Drop(node);
    return std::shared_ptr<Module>(cloned);
  }

  virtual bool equals(const Module *other) const override {
    return other->get_type() == type;
  }
};

} // namespace x86
} // namespace synapse
