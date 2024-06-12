#pragma once

#include "tofino_module.h"

#include "else.h"
#include "then.h"

namespace synapse {
namespace tofino {

class If : public TofinoModule {
private:
  klee::ref<klee::Expr> original_condition;

  // AND-changed conditions.
  // Every condition must be met to be equivalent to the original condition.
  // This will later be split into multiple branch conditions, each evaluating
  // each sub condition.
  std::vector<klee::ref<klee::Expr>> conditions;

public:
  If(const bdd::Node *node, klee::ref<klee::Expr> _original_condition,
     const std::vector<klee::ref<klee::Expr>> &_conditions)
      : TofinoModule(ModuleType::Tofino_If, "If", node),
        original_condition(_original_condition), conditions(_conditions) {}

  virtual void visit(EPVisitor &visitor, const EP *ep,
                     const EPNode *ep_node) const override {
    visitor.visit(ep, ep_node, this);
  }

  virtual Module *clone() const {
    If *cloned = new If(node, original_condition, conditions);
    return cloned;
  }

  const std::vector<klee::ref<klee::Expr>> &get_conditions() const {
    return conditions;
  }
};

class IfGenerator : public TofinoModuleGenerator {
public:
  IfGenerator() : TofinoModuleGenerator(ModuleType::Tofino_If, "If") {}

protected:
  virtual std::vector<const EP *>
  process_node(const EP *ep, const bdd::Node *node) const override {
    std::vector<const EP *> new_eps;

    if (node->get_type() != bdd::NodeType::BRANCH) {
      return new_eps;
    }

    const bdd::Branch *branch_node = static_cast<const bdd::Branch *>(node);

    if (is_parser_condition(branch_node)) {
      return new_eps;
    }

    const TNA &tna = get_tna(ep);

    klee::ref<klee::Expr> condition = branch_node->get_condition();
    std::vector<klee::ref<klee::Expr>> conditions = split_condition(condition);

    for (klee::ref<klee::Expr> sub_condition : conditions) {
      if (!tna.condition_meets_phv_limit(sub_condition)) {
        assert(false && "TODO: deal with this");
        return new_eps;
      }
    }

    assert(branch_node->get_on_true());
    assert(branch_node->get_on_false());

    Module *if_module = new If(node, condition, conditions);
    Module *then_module = new Then(node);
    Module *else_module = new Else(node);

    EPNode *if_node = new EPNode(if_module);
    EPNode *then_node = new EPNode(then_module);
    EPNode *else_node = new EPNode(else_module);

    if_node->set_children({then_node, else_node});
    then_node->set_prev(if_node);
    else_node->set_prev(if_node);

    EPLeaf then_leaf(then_node, branch_node->get_on_true());
    EPLeaf else_leaf(else_node, branch_node->get_on_false());

    EP *new_ep = new EP(*ep);
    new_eps.push_back(new_ep);

    new_ep->update_node_constraints(then_node, else_node, condition);
    new_ep->process_leaf(if_node, {then_leaf, else_leaf});

    return new_eps;
  }

private:
  std::vector<klee::ref<klee::Expr>>
  split_condition(klee::ref<klee::Expr> condition) const {
    std::vector<klee::ref<klee::Expr>> conditions;

    switch (condition->getKind()) {
    case klee::Expr::Kind::And: {
      klee::ref<klee::Expr> lhs = condition->getKid(0);
      klee::ref<klee::Expr> rhs = condition->getKid(1);

      std::vector<klee::ref<klee::Expr>> lhs_conds = split_condition(lhs);
      std::vector<klee::ref<klee::Expr>> rhs_conds = split_condition(rhs);

      conditions.insert(conditions.end(), lhs_conds.begin(), lhs_conds.end());
      conditions.insert(conditions.end(), rhs_conds.begin(), rhs_conds.end());
    } break;
    case klee::Expr::Kind::Or: {
      assert(false && "TODO");
    } break;
    default: {
      conditions.push_back(condition);
    }
    }

    return conditions;
  }
};

} // namespace tofino
} // namespace synapse
