#pragma once

#include "else.h"
#include "ignore.h"
#include "then.h"
#include "tofino_module.h"

namespace synapse {
namespace targets {
namespace tofino {

typedef std::vector<klee::ref<klee::Expr>> conditions_t;

class If : public TofinoModule {
private:
  conditions_t conditions;

public:
  If() : TofinoModule(ModuleType::Tofino_If, "If") {}

  If(BDD::Node_ptr node, conditions_t _conditions)
      : TofinoModule(ModuleType::Tofino_If, "If", node),
        conditions(_conditions) {}

private:
  // Tofino compiler complains if we access more than 4 bytes of the packet on
  // the same if statement.
  bool pass_compiler_packet_bytes_constraint(conditions_t _conditions) const {
    constexpr int MAX_PACKET_BYTES = 4;

    for (auto condition : _conditions) {
      kutil::RetrieveSymbols retriever;
      retriever.visit(condition);

      auto chunks = retriever.get_retrieved_packet_chunks();

      if (chunks.size() > MAX_PACKET_BYTES) {
        return false;
      }
    }

    return true;
  }

  conditions_t split_condition(klee::ref<klee::Expr> expr) const {
    auto _conditions = conditions_t{expr};

    auto and_expr_finder = [](klee::ref<klee::Expr> e) {
      return e->getKind() == klee::Expr::And;
    };

    while (1) {
      auto found_it =
          std::find_if(_conditions.begin(), _conditions.end(), and_expr_finder);

      if (found_it == _conditions.end()) {
        break;
      }

      auto and_expr = *found_it;
      _conditions.erase(found_it);

      auto lhs = and_expr->getKid(0);
      auto rhs = and_expr->getKid(1);

      _conditions.push_back(lhs);
      _conditions.push_back(rhs);
    }

    // minor simplification step

    for (auto &condition : _conditions) {
      if (condition->getKind() == klee::Expr::ZExt ||
          condition->getKind() == klee::Expr::SExt) {
        condition = condition->getKid(0);
      }
    }

    return _conditions;
  }

  processing_result_t process(const ExecutionPlan &ep,
                              BDD::Node_ptr node) override {
    processing_result_t result;

    auto casted = BDD::cast_node<BDD::Branch>(node);

    if (!casted) {
      return result;
    }

    assert(!casted->get_condition().isNull());
    auto _condition = casted->get_condition();

    if (is_parser_condition(node)) {
      return result;
    }

    auto _conditions = conditions_t{_condition};

    if (!pass_compiler_packet_bytes_constraint(_conditions)) {
      _conditions = split_condition(_condition);

      if (!pass_compiler_packet_bytes_constraint(_conditions)) {
        Log::wrn() << "Unable to implement branching condition in Tofino.\n";
        Log::wrn() << "Only 4 packet bytes are allowed in condition.\n";
        return result;
      }
    }

    auto new_if_module = std::make_shared<If>(node, _conditions);

    auto new_then_module = std::make_shared<Then>(node);
    auto new_else_module = std::make_shared<Else>(node);

    auto if_leaf = ExecutionPlan::leaf_t(new_if_module, nullptr);
    auto then_leaf =
        ExecutionPlan::leaf_t(new_then_module, casted->get_on_true());
    auto else_leaf =
        ExecutionPlan::leaf_t(new_else_module, casted->get_on_false());

    std::vector<ExecutionPlan::leaf_t> if_leaves{if_leaf};
    std::vector<ExecutionPlan::leaf_t> then_else_leaves{then_leaf, else_leaf};

    auto ep_if = ep.add_leaves(if_leaves);
    auto ep_if_then_else = ep_if.add_leaves(then_else_leaves);

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
    auto cloned = new If(node, conditions);
    return std::shared_ptr<Module>(cloned);
  }

  virtual bool equals(const Module *other) const override {
    if (other->get_type() != type) {
      return false;
    }

    auto other_cast = static_cast<const If *>(other);

    auto other_conditions = other_cast->get_conditions();

    if (conditions.size() != other_conditions.size()) {
      return false;
    }

    for (auto i = 0u; i < conditions.size(); i++) {
      if (!kutil::solver_toolbox.are_exprs_always_equal(conditions[i],
                                                        other_conditions[i])) {
        return false;
      }
    }

    return true;
  }

  const conditions_t &get_conditions() const { return conditions; }
};
} // namespace tofino
} // namespace targets
} // namespace synapse
