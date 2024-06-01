#pragma once

#include "tofino_module.h"

#include "else.h"
#include "then.h"

namespace synapse {
namespace tofino {

class If : public TofinoModule {
private:
  klee::ref<klee::Expr> condition;

public:
  If(const bdd::Node *node, klee::ref<klee::Expr> _condition)
      : TofinoModule(ModuleType::Tofino_If, "If", node), condition(_condition) {
  }

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
    klee::ref<klee::Expr> condition = branch_node->get_condition();

    const TofinoContext *tofino_ctx = get_tofino_ctx(ep);
    if (!tofino_ctx->condition_meets_phv_limit(condition)) {
      return new_eps;
    }

    if (is_parser_condition(node)) {
      return new_eps;
    }

    assert(branch_node->get_on_true());
    assert(branch_node->get_on_false());

    Module *if_module = new If(node, condition);
    Module *then_module = new Then(node);
    Module *else_module = new Else(node);

    EPNode *if_node = new EPNode(if_module);
    EPNode *then_node = new EPNode(then_module);
    EPNode *else_node = new EPNode(else_module);

    EPLeaf then_leaf(then_node, branch_node->get_on_true());
    EPLeaf else_leaf(else_node, branch_node->get_on_false());

    EP *new_ep = new EP(*ep);
    new_eps.push_back(new_ep);

    new_ep->process_leaf(if_node, {then_leaf, else_leaf});

    return new_eps;
  }
};

} // namespace tofino
} // namespace synapse
