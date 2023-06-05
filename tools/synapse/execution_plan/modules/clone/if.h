#pragma once


#include "call-paths-to-bdd.h"
#include "clone_module.h"
#include "parser/infrastructure.h"
#include "then.h"
#include "else.h"
#include "memory_bank.h"
#include <cstdio>

namespace synapse {
namespace targets {
namespace clone {

class If: public CloneModule {
private:

public:
  If() : CloneModule(ModuleType::Clone_If, "If") {
 //   next_target = TargetType::x86;
  }

  If(BDD::Node_ptr node)
    : CloneModule(ModuleType::Clone_If, "If", node) {
    //  next_target = TargetType::x86;
  }

private: 
  BDD::Node_ptr get_drop(BDD::Node_ptr prev) {
    return BDD::Node_ptr(new BDD::ReturnProcess(0, prev, {}, 0, BDD::ReturnProcess::Operation::DROP));
  }

  unsigned extract_port(const BDD::Branch* branch) {
    auto kind = branch->get_condition()->getKind();
    assert(kind == klee::Expr::Kind::Eq);

    auto condition = branch->get_condition();
    auto right = condition->getKid(1);
    assert(right->getKind() == klee::Expr::Kind::Constant);
    auto casted = static_cast<klee::ConstantExpr*>(right.get());

    return casted->getZExtValue();
  }

  klee::ref<klee::Expr> replace_port(BDD::Node_ptr node, unsigned port) {
    BDD::Branch* branch = static_cast<BDD::Branch*>(node.get());
    auto condition = branch->get_condition();
    auto left = condition->getKid(0);
    auto right = condition->getKid(1);
    assert(right->getKind() == klee::Expr::Kind::Constant);

    auto new_right = klee::ConstantExpr::create(port, 32);
    auto new_condition = klee::EqExpr::create(left, new_right);
    branch->set_condition(new_condition);

    return branch->get_condition();
  }

  unsigned get_local_port(std::shared_ptr<Clone::Infrastructure> infra, BDD::Node_ptr node) {
    BDD::Branch* casted = static_cast<BDD::Branch*>(node.get());
    unsigned value = extract_port(casted);
    auto port = infra->get_port(value);
    unsigned local_port = port->get_local_port();

    return local_port;
  }

  std::map<std::string, std::vector<BDD::Node_ptr>> get_nodes_per_device(std::shared_ptr<Clone::Infrastructure> infra, BDD::Node_ptr node) {
      std::map<std::string, std::vector<BDD::Node_ptr>> m;

      while(node->get_type() == BDD::Node::NodeType::BRANCH) {
        auto casted = BDD::cast_node<BDD::Branch>(node);
        unsigned value = extract_port(casted);
        auto port = infra->get_port(value);
        const std::string& name = port->get_device()->get_id();

        if(m.find(name) == m.end()) {
          m.emplace(name, std::vector<BDD::Node_ptr>());
        }
        m.at(name).push_back(node);

        node = casted->get_on_false();
      }

      return m;
  }

  void init_starting_points(const ExecutionPlan &ep, BDD::Node_ptr origin) {
    auto mb = ep.get_memory_bank<CloneMemoryBank>(TargetType::CloNe);

    auto infra = ep.get_infrastructure();
    assert(infra != nullptr);

    auto branches = get_nodes_per_device(infra, origin);   
    for(auto& p_branches: branches) {
      auto &v_nodes = p_branches.second;
      const std::string &device_name = p_branches.first;

      auto it = v_nodes.begin();
      assert(v_nodes.size() > 0);
      BDD::Node_ptr node = *it;

      while (it != v_nodes.end()) {
        node = *it;
        unsigned local_port = get_local_port(infra, node);
        replace_port(node, local_port);
        mb->add_starting_point(device_name, node);
        ++it;
      }

      BDD::Branch* casted = static_cast<BDD::Branch*>(node.get());
      auto drop = get_drop(node);
      casted->replace_on_false(drop);
    }
  }

  processing_result_t process(const ExecutionPlan &ep,
                              BDD::Node_ptr node) override {
    processing_result_t result;

    if(node->get_type() != BDD::Node::NodeType::BRANCH) {
      return result;
    }

    auto mb = ep.get_memory_bank<CloneMemoryBank>(TargetType::CloNe);
    if(!mb->has_starting_points()) {
      init_starting_points(ep, node);
    }
    assert(mb->get_next_node()->get_id() == node->get_id());

    auto infra = ep.get_infrastructure();
    std::string device_name = mb->get_next_device();
    auto device = infra->get_device(device_name);
    std::string architecture = device->get_type();
    auto target = string_to_target_type[architecture];

    auto if_module = std::make_shared<If>(node);
    auto new_ep = ep.ignore_leaf(node, target, false);

    result.module = if_module;
    result.next_eps.push_back(new_ep);

    return result;
  }

public:
 virtual void visit(ExecutionPlanVisitor &visitor,
                     const ExecutionPlanNode *ep_node) const override {
    visitor.visit(ep_node, this);
  }

  virtual Module_ptr clone() const override {
    auto cloned = new If(node);
    return std::shared_ptr<Module>(cloned);
  }

  virtual bool equals(const Module *other) const override {
    return other->get_type() != type;
  }
};

} // namespace clone
} // namespace targets
} // namespace synapse