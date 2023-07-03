#pragma once

#include "clone.h"
#include "call-paths-to-bdd.h"
#include "models/device.h"
#include "x86_module.h"
#include "../../target.h"

namespace synapse {
namespace targets {
namespace x86 {

typedef uint16_t cpu_code_path_t;

class SendToDevice : public x86Module {
private:
  cpu_code_path_t cpu_code_path;
  unsigned outgoing_port;
  unsigned incoming_port;
  Target_ptr destination_target;
public:
  SendToDevice() : x86Module(ModuleType::x86_SendToDevice, "SendToDevice") {}

  SendToDevice(BDD::Node_ptr node, cpu_code_path_t _cpu_code_path, unsigned _outgoing_port, unsigned incoming_port, Target_ptr _destination_target)
      : x86Module(ModuleType::x86_SendToDevice, "SendToDevice", node), 
      cpu_code_path(_cpu_code_path), outgoing_port(_outgoing_port), incoming_port(incoming_port), 
      destination_target(_destination_target) {}

private:
  //                  outgoing   incoming   next device
  std::pair<std::pair<unsigned, unsigned>, Clone::DevicePtr> get_neighbour_from_node(const ExecutionPlan &ep, BDD::Node_ptr node) {
    switch(node->get_type()) {
      case BDD::Node::NodeType::RETURN_PROCESS: {
        const BDD::ReturnProcess* casted = BDD::cast_node<BDD::ReturnProcess>(node);
        auto infra = ep.get_infrastructure();
        auto current_device = ep.get_current_target()->instance->name;
        auto desination_port = infra->get_port(casted->get_return_value());
        auto destination_device = desination_port->get_device()->get_id();
        
        auto routing_table = infra->get_routing_table(current_device);
        if(!routing_table.count(destination_device)) {
          assert(false && "TODO");
        }
        auto neighbour_device = routing_table.at(destination_device);
        auto neighbour = infra->get_device(current_device)->get_neighbour(neighbour_device);

        return std::make_pair(std::make_pair(neighbour.first, neighbour.second), infra->get_device(neighbour_device));
        break;
      }
      default:
        break;
    }

    return std::make_pair(std::make_pair(-1, -1), nullptr);
  }

  bool evaluate_return_process(const ExecutionPlan &ep, const BDD::ReturnProcess* node) {
    switch(node->get_return_operation()) {
      case BDD::ReturnProcess::Operation::FWD: {
        auto infra = ep.get_infrastructure();
        std::string instance = ep.get_current_target()->instance->name;
        auto port = infra->get_port(node->get_return_value());

        return port->get_device()->get_id() != instance;
      }
      default: {
        break;
      }
    }

    return false;
  }

  bool should_change_device(const ExecutionPlan &ep, BDD::Node_ptr node) {
    if(ep.get_infrastructure() == nullptr) {
      return false;
    }

    if(!ep.has_target_type(TargetType::CloNe)) {
      return false;
    }

    switch(node->get_type()) {
      case BDD::Node::NodeType::RETURN_PROCESS: {
        const BDD::ReturnProcess* casted = BDD::cast_node<BDD::ReturnProcess>(node);
        
        return evaluate_return_process(ep, casted);
      }
      default:
        break;
    }

    return false;
  }

  processing_result_t process(const ExecutionPlan &ep, BDD::Node_ptr node) override {
    processing_result_t result;

    if(!should_change_device(ep, node)) {
      return result;
    }

    auto neighbour = get_neighbour_from_node(ep, node);
    assert(neighbour.second);

    TargetType next_target_type = string_to_target_type[neighbour.second->get_type()];
    auto next_target = ep.get_target(next_target_type, neighbour.second->get_id());
    auto _code_path = node->get_id();

    auto send_to_device = std::make_shared<SendToDevice>(node, _code_path, neighbour.first.first, neighbour.first.second, next_target);

    auto ep_cloned = ep.clone(true);
    auto new_ep = ep_cloned.add_leaves(send_to_device, node, false, false, next_target);
    //new_ep.replace_active_leaf_node(node, false);

    result.module = send_to_device;
    result.next_eps.push_back(new_ep);

    return result;
  }

public:
  virtual void visit(ExecutionPlanVisitor &visitor, const ExecutionPlanNode *ep_node) const override {
    visitor.visit(ep_node, this);
  }

  virtual Module_ptr clone() const override {
    auto cloned = new SendToDevice(node, cpu_code_path, outgoing_port, incoming_port, destination_target);
    return std::shared_ptr<Module>(cloned);
  }

  virtual bool equals(const Module *other) const override {
    if (other->get_type() != type) {
      return false;
    }

    const SendToDevice *casted = static_cast<const SendToDevice*>(other);

    if (casted->get_outgoing_port() != outgoing_port) {
      return false;
    }

    return true;
  }

  unsigned get_outgoing_port() const {
    return outgoing_port;
  }

  unsigned get_incoming_port() const {
    return incoming_port;
  }

  cpu_code_path_t get_cpu_code_path() const { return cpu_code_path; }
};

} // namespace x86
} // namespace targets
} // namespace synapse
