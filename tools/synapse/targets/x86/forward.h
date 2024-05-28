#pragma once

#include "x86_module.h"

namespace synapse {
namespace x86 {

class Forward : public x86Module {
private:
  int port;

public:
  Forward() : x86Module(ModuleType::x86_Forward, "Forward") {}

  Forward(const bdd::Node *node, int _port)
      : x86Module(ModuleType::x86_Forward, "Forward", node), port(_port) {}

private:
  generated_data_t process(const EP *ep, const bdd::Node *node) override {
    generated_data_t result;

    auto casted = static_cast<const bdd::Route *>(node);

    if (!casted) {
      return result;
    }

    if (casted->get_operation() == bdd::RouteOperation::FWD) {
      auto _port = casted->get_dst_device();

      auto new_module = std::make_shared<Forward>(node, _port);
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
    auto cloned = new Forward(node, port);
    return std::shared_ptr<Module>(cloned);
  }

  virtual bool equals(const Module *other) const override {
    if (other->get_type() != type) {
      return false;
    }

    auto other_cast = static_cast<const Forward *>(other);

    if (port != other_cast->get_port()) {
      return false;
    }

    return true;
  }

  int get_port() const { return port; }
};

} // namespace x86
} // namespace synapse
