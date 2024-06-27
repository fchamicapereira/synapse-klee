#pragma once

#include "x86_module.h"

#include "else.h"
#include "then.h"

namespace synapse {
namespace x86 {

class If : public x86Module {
private:
  klee::ref<klee::Expr> condition;

public:
  If(const bdd::Node *node, klee::ref<klee::Expr> _condition)
      : x86Module(ModuleType::x86_If, "If", node), condition(_condition) {}

  virtual void visit(EPVisitor &visitor, const EP *ep,
                     const EPNode *ep_node) const override {
    visitor.visit(ep, ep_node, this);
  }

  virtual Module *clone() const {
    If *cloned = new If(node, condition);
    return cloned;
  }

  klee::ref<klee::Expr> get_condition() const { return condition; }
};

class IfGenerator : public x86ModuleGenerator {
public:
  IfGenerator() : x86ModuleGenerator(ModuleType::x86_If, "If") {}

protected:
  bool bdd_node_match_pattern(const bdd::Node *node) const {
    if (node->get_type() != bdd::NodeType::BRANCH) {
      return false;
    }

    return true;
  }

  virtual std::optional<speculation_t>
  speculate(const EP *ep, const bdd::Node *node,
            const constraints_t &current_speculative_constraints,
            const Context &current_speculative_ctx) const override {
    if (bdd_node_match_pattern(node))
      return current_speculative_ctx;
    return std::nullopt;
  }

  virtual std::vector<const EP *>
  process_node(const EP *ep, const bdd::Node *node) const override {
    std::vector<const EP *> new_eps;

    if (!bdd_node_match_pattern(node)) {
      return new_eps;
    }

    const bdd::Branch *branch_node = static_cast<const bdd::Branch *>(node);

    klee::ref<klee::Expr> condition = branch_node->get_condition();

    assert(branch_node->get_on_true());
    assert(branch_node->get_on_false());

    Module *if_module = new If(node, condition);
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
};

} // namespace x86
} // namespace synapse
