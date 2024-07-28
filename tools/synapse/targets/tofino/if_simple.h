#pragma once

#include "tofino_module.h"

#include "else.h"
#include "then.h"

namespace synapse {
namespace tofino {

class IfSimple : public TofinoModule {
private:
  klee::ref<klee::Expr> condition;

public:
  IfSimple(const bdd::Node *node, klee::ref<klee::Expr> _condition)
      : TofinoModule(ModuleType::Tofino_IfSimple, "IfSimple", node),
        condition(_condition) {}

  virtual void visit(EPVisitor &visitor, const EP *ep,
                     const EPNode *ep_node) const override {
    visitor.visit(ep, ep_node, this);
  }

  virtual Module *clone() const {
    IfSimple *cloned = new IfSimple(node, condition);
    return cloned;
  }

  klee::ref<klee::Expr> get_condition() const { return condition; }
};

class IfSimpleGenerator : public TofinoModuleGenerator {
public:
  IfSimpleGenerator()
      : TofinoModuleGenerator(ModuleType::Tofino_IfSimple, "IfSimple") {}

protected:
  virtual std::optional<speculation_t>
  speculate(const EP *ep, const bdd::Node *node,
            const Context &ctx) const override {
    if (node->get_type() != bdd::NodeType::BRANCH) {
      return std::nullopt;
    }

    return ctx;
  }

  virtual std::vector<__generator_product_t>
  process_node(const EP *ep, const bdd::Node *node) const override {
    std::vector<__generator_product_t> products;

    if (node->get_type() != bdd::NodeType::BRANCH) {
      return products;
    }

    const bdd::Branch *branch_node = static_cast<const bdd::Branch *>(node);

    if (is_parser_condition(branch_node)) {
      return products;
    }

    klee::ref<klee::Expr> condition = branch_node->get_condition();
    std::vector<klee::ref<klee::Expr>> conditions = split_condition(condition);

    if (conditions.size() > 1) {
      return products;
    }

    assert(branch_node->get_on_true());
    assert(branch_node->get_on_false());

    Module *if_module = new IfSimple(node, condition);
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
    products.emplace_back(new_ep);

    new_ep->update_node_constraints(then_node, else_node, condition);
    new_ep->process_leaf(if_node, {then_leaf, else_leaf});

    return products;
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
