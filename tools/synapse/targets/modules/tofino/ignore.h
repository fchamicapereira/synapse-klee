#pragma once

#include "tofino_module.h"

#include <unordered_map>

namespace synapse {
namespace targets {
namespace tofino {

class Ignore : public TofinoModule {
private:
  typedef bool (Ignore::*should_ignore_jury_ptr)(const ExecutionPlan &ep,
                                                 BDD::Node_ptr node,
                                                 const BDD::Call *casted) const;

  std::unordered_map<std::string, should_ignore_jury_ptr> functions_to_ignore;

public:
  Ignore() : TofinoModule(ModuleType::Tofino_Ignore, "Ignore") {
    functions_to_ignore = {
        {BDD::symbex::FN_CURRENT_TIME, &Ignore::always_ignore},
        {BDD::symbex::FN_ETHER_HASH, &Ignore::always_ignore},
        {BDD::symbex::FN_SET_CHECKSUM, &Ignore::always_ignore},
        {BDD::symbex::FN_VECTOR_RETURN, &Ignore::read_only_vector_op},
    };
  }

  Ignore(BDD::Node_ptr node)
      : TofinoModule(ModuleType::Tofino_Ignore, "Ignore", node) {}

private:
  bool always_ignore(const ExecutionPlan &ep, BDD::Node_ptr node,
                     const BDD::Call *casted) const {
    return true;
  }

  call_t get_previous_vector_borrow(const ExecutionPlan &ep, BDD::Node_ptr node,
                                    klee::ref<klee::Expr> wanted_vector) const {
    auto prev_vector_borrows = get_prev_fn(ep, node, BDD::symbex::FN_VECTOR_BORROW);

    for (auto prev_node : prev_vector_borrows) {
      assert(prev_node->get_type() == BDD::Node::NodeType::CALL);

      auto call_node = static_cast<const BDD::Call *>(prev_node.get());
      auto call = call_node->get_call();

      if (call.function_name != BDD::symbex::FN_VECTOR_BORROW) {
        continue;
      }

      auto vector = call.args[BDD::symbex::FN_VECTOR_ARG_VECTOR].expr;
      auto eq =
          kutil::solver_toolbox.are_exprs_always_equal(vector, wanted_vector);

      if (eq) {
        return call;
      }
    }

    assert(false);
    exit(1);
  }

  bool read_only_vector_op(const ExecutionPlan &ep, BDD::Node_ptr node,
                           const BDD::Call *casted) const {
    assert(casted);
    auto call = casted->get_call();
    assert(call.function_name == BDD::symbex::FN_VECTOR_RETURN);

    assert(!call.args[BDD::symbex::FN_VECTOR_ARG_VECTOR].expr.isNull());
    assert(!call.args[BDD::symbex::FN_VECTOR_ARG_VALUE].in.isNull());

    auto vector = call.args[BDD::symbex::FN_VECTOR_ARG_VECTOR].expr;
    auto cell_after = call.args[BDD::symbex::FN_VECTOR_ARG_VALUE].in;

    auto vector_borrow = get_previous_vector_borrow(ep, node, vector);
    auto cell_before = vector_borrow.extra_vars[BDD::symbex::FN_VECTOR_EXTRA].second;

    assert(cell_before->getWidth() == cell_after->getWidth());
    auto eq =
        kutil::solver_toolbox.are_exprs_always_equal(cell_before, cell_after);

    auto modifies_cell = !eq;
    return !modifies_cell;
  }

  bool recall_to_ignore(const ExecutionPlan &ep, BDD::Node_ptr node) {
    auto mb = ep.get_memory_bank();
    return mb->check_if_can_be_ignored(node);
  }

  processing_result_t process(const ExecutionPlan &ep,
                              BDD::Node_ptr node) override {
    processing_result_t result;

    auto casted = BDD::cast_node<BDD::Call>(node);

    if (!casted) {
      return result;
    }

    auto call = casted->get_call();

    if (recall_to_ignore(ep, node)) {
      auto new_module = std::make_shared<Ignore>(node);
      auto new_ep = ep.ignore_leaf(node->get_next(), TargetType::Tofino);

      result.module = new_module;
      result.next_eps.push_back(new_ep);

      return result;
    }

    auto found_it = functions_to_ignore.find(call.function_name);

    if (found_it == functions_to_ignore.end()) {
      return result;
    }

    auto should_ignore_jury = found_it->second;
    auto should_ignore = (this->*should_ignore_jury)(ep, node, casted);

    if (should_ignore) {
      auto new_module = std::make_shared<Ignore>(node);
      auto new_ep = ep.ignore_leaf(node->get_next(), TargetType::Tofino);

      result.module = new_module;
      result.next_eps.push_back(new_ep);
    }

    return result;
  }

public:
  virtual void visit(ExecutionPlanVisitor &visitor,
                     const ExecutionPlanNode *ep_node) const override {
    visitor.visit(ep_node, this);
  }

  virtual Module_ptr clone() const override {
    auto cloned = new Ignore(node);
    return std::shared_ptr<Module>(cloned);
  }

  virtual bool equals(const Module *other) const override {
    return other->get_type() == type;
  }
};
} // namespace tofino
} // namespace targets
} // namespace synapse
