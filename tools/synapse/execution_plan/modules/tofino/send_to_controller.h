#pragma once

#include "../module.h"
#include "../x86_tofino/packet_parse_cpu.h"

namespace synapse {
namespace targets {
namespace tofino {

class SendToController : public Module {
private:
  uint64_t cpu_code_path;

public:
  SendToController()
      : Module(ModuleType::Tofino_SendToController, TargetType::Tofino,
               "SendToController") {
    next_target = TargetType::x86_Tofino;
  }

  SendToController(BDD::BDDNode_ptr node, uint64_t _cpu_code_path)
      : Module(ModuleType::Tofino_SendToController, TargetType::Tofino,
               "SendToController", node),
        cpu_code_path(_cpu_code_path) {
    next_target = TargetType::x86_Tofino;
  }

private:
  void replace_next(BDD::BDDNode_ptr prev, BDD::BDDNode_ptr old_next,
                    BDD::BDDNode_ptr new_next) const {
    assert(prev);
    assert(old_next);
    assert(new_next);

    if (prev->get_type() == BDD::Node::NodeType::BRANCH) {
      auto branch_node = static_cast<BDD::Branch *>(prev.get());

      if (branch_node->get_on_true()->get_id() == old_next->get_id()) {
        branch_node->replace_on_true(new_next);
      } else {
        branch_node->replace_on_false(new_next);
      }

      new_next->replace_prev(prev);
      return;
    }

    prev->replace_next(new_next);
    new_next->replace_prev(prev);
  }

  BDD::BDDNode_ptr clone_calls(ExecutionPlan &ep,
                               BDD::BDDNode_ptr current) const {
    assert(current);

    if (!current->get_prev()) {
      return current;
    }

    auto node = current;
    auto root = current;
    auto next = current->get_next();
    auto prev = current->get_prev();

    auto &bdd = ep.get_bdd();

    while (node->get_prev()) {
      node = node->get_prev();

      if (node->get_type() == BDD::Node::NodeType::CALL) {
        auto clone = node->clone();

        clone->replace_next(root);
        clone->replace_prev(nullptr);

        root->replace_prev(clone);

        auto id = bdd.get_id();
        clone->update_id(id);
        bdd.set_id(id + 1);

        root = clone;
      }
    }

    replace_next(prev, current, root);

    return root;
  }

  processing_result_t process(const ExecutionPlan &ep, BDD::BDDNode_ptr node) {
    processing_result_t result;

    if (!ep.has_target(TargetType::x86_Tofino)) {
      return result;
    }

    auto ep_cloned = ep.clone(true);
    auto &bdd = ep_cloned.get_bdd();
    auto node_cloned = bdd.get_node_by_id(node->get_id());

    auto next_node = clone_calls(ep_cloned, node_cloned);
    auto _code_path = node->get_id();

    auto send_to_controller =
        std::make_shared<SendToController>(node_cloned, _code_path);

    auto new_ep =
        ep_cloned.add_leaves(send_to_controller, next_node, false, false);
    new_ep.replace_active_leaf_node(next_node, false);

    result.module = send_to_controller;
    result.next_eps.push_back(new_ep);

    return result;
  }

  processing_result_t process_branch(const ExecutionPlan &ep,
                                     BDD::BDDNode_ptr node,
                                     const BDD::Branch *casted) override {
    return process(ep, node);
  }

  processing_result_t process_call(const ExecutionPlan &ep,
                                   BDD::BDDNode_ptr node,
                                   const BDD::Call *casted) override {
    return process(ep, node);
  }

public:
  virtual void visit(ExecutionPlanVisitor &visitor) const override {
    visitor.visit(this);
  }

  virtual Module_ptr clone() const override {
    auto cloned = new SendToController(node, cpu_code_path);
    return std::shared_ptr<Module>(cloned);
  }

  virtual bool equals(const Module *other) const override {
    if (other->get_type() != type) {
      return false;
    }

    auto other_cast = static_cast<const SendToController *>(other);

    if (cpu_code_path != other_cast->cpu_code_path) {
      return false;
    }

    return true;
  }

  uint64_t get_cpu_code_path() const { return cpu_code_path; }
};
} // namespace tofino
} // namespace targets
} // namespace synapse
