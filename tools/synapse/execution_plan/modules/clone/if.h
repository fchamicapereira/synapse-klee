#pragma once


#include "call-paths-to-bdd.h"
#include "clone_module.h"
#include "klee/Expr.h"
#include "parser/infrastructure.h"
#include "then.h"
#include "else.h"
#include "memory_bank.h"
#include <map>
#include <cstdio>

namespace synapse {
namespace targets {
namespace clone {

class If: public CloneModule {
private:

public:
  If() : CloneModule(ModuleType::Clone_If, "If") {}

  If(BDD::Node_ptr node)
    : CloneModule(ModuleType::Clone_If, "If", node) {}

private: 
  BDD::Node_ptr get_drop(BDD::Node_ptr prev, BDD::node_id_t id) {
    return BDD::Node_ptr(new BDD::ReturnProcess(id, prev, {}, 0, BDD::ReturnProcess::Operation::DROP));
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

  // replace global ports with real ports
  void replace_ports(std::map<int, int> ports_map, BDD::Node_ptr first_node) {
    std::vector<BDD::Node_ptr> nodes{first_node};
    BDD::Node_ptr node;

    while (nodes.size()) {
      node = nodes[0];
      nodes.erase(nodes.begin());

      if (node->get_type() == BDD::Node::NodeType::RETURN_PROCESS) {
        auto return_process = static_cast<BDD::ReturnProcess *>(node.get());
        if(return_process->get_return_operation() == BDD::ReturnProcess::Operation::FWD) {
          assert(ports_map.find(return_process->get_return_value()) != ports_map.end());
          return_process->set_return_value(ports_map.at(return_process->get_return_value()));
        }
      }

      if (node->get_type() == BDD::Node::NodeType::BRANCH) {
        auto branch_node = static_cast<BDD::Branch *>(node.get());

        nodes.push_back(branch_node->get_on_true());
        nodes.push_back(branch_node->get_on_false());
      } else if (node->get_next()) {
        nodes.push_back(node->get_next());
      }
    }
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

  void init_starting_points(const ExecutionPlan &ep, BDD::Node_ptr origin) {
    // Variable initialisation
    auto mb = ep.get_memory_bank<CloneMemoryBank>(TargetType::CloNe);
    auto infra = ep.get_infrastructure();
    auto bdd = ep.get_bdd();
    BDD::node_id_t id = bdd.get_id() + 1;
     std::map<std::string, std::vector<BDD::Node_ptr>> device_to_nodes;

    std::map<int, int> m_ports;
    for(auto& p: infra->get_ports()) {
      auto port = p.second;
      m_ports[port->get_global_port()] = port->get_local_port();
    }

    auto node = origin;
    while(node->get_type() == BDD::Node::NodeType::BRANCH) {
      auto casted = BDD::cast_node<BDD::Branch>(node);
      unsigned value = extract_port(casted);
      auto port = infra->get_port(value);
      const std::string& arch = port->get_device()->get_type();
      const std::string& device = port->get_device()->get_id();

      if(device_to_nodes.find(device) == device_to_nodes.end()) {
        device_to_nodes.emplace(device, std::vector<BDD::Node_ptr>());
        mb->add_starting_point(node, arch, name);
      }
      device_to_nodes.at(device).push_back(node);
      node = casted->get_on_false();
    }

    for(auto& starting_node: device_to_nodes) {
      const std::string& device_name = starting_node.first;
      auto &v_nodes = starting_node.second;

      auto it = v_nodes.begin();
      assert(v_nodes.size() > 0);
      auto device = infra->get_device(device_name);
      std::string arch = device->get_type();

      BDD::Node_ptr node = *it;
      node->disconnect_prev();
      while (it != v_nodes.end()) {
        node = *it;
        unsigned local_port = get_local_port(infra, node);
        replace_port(node, local_port);
        ++it;
      }

      BDD::Branch* casted = static_cast<BDD::Branch*>(node.get());
      auto drop = get_drop(node, id++);
      casted->replace_on_false(drop);

      // TODO: move else where, repeat this loop
      replace_ports(m_ports, *v_nodes.begin());
    }
  }

  processing_result_t process(const ExecutionPlan &ep,
                              BDD::Node_ptr node) override {
    processing_result_t result;

    if(node->get_type() != BDD::Node::NodeType::BRANCH) {
      return result;
    }

    auto mb = ep.get_memory_bank<CloneMemoryBank>(TargetType::CloNe);
    if(!mb->is_initialised()) {
      init_starting_points(ep, node);
    }

    if(mb->get_next_starting_point() == nullptr) {
      return result;
    }

    mb->pop_starting_point();
    auto start_point =  mb->get_next_starting_point();
    
    BDD::Node_ptr next = start_point != nullptr ? start_point->node : nullptr;
    if(next == nullptr) {
      next = get_drop(nullptr, -2);
    }

    auto new_if_module = std::make_shared<If>(node);
    auto new_then_module = std::make_shared<Then>(node);
    auto new_else_module = std::make_shared<Else>(node);

    auto if_leaf = ExecutionPlan::leaf_t(new_if_module, nullptr);
    auto then_leaf =
        ExecutionPlan::leaf_t(new_then_module, node);
    auto else_leaf =
        ExecutionPlan::leaf_t(new_else_module, next);

    std::vector<ExecutionPlan::leaf_t> if_leaves{if_leaf};
    std::vector<ExecutionPlan::leaf_t> then_else_leaves{then_leaf, else_leaf};

    auto ep_if = ep.add_leaves(if_leaves, false, false);
    auto ep_if_then_else = ep_if.add_leaves(then_else_leaves, false, false);

    result.module = new_if_module;
    result.next_eps.push_back(ep_if_then_else);

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