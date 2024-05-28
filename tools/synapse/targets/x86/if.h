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

  virtual void visit(EPVisitor &visitor, const EPNode *ep_node) const override {
    visitor.visit(ep_node, this);
  }

  virtual Module *clone() const {
    If *cloned = new If(node, condition);
    return cloned;
  }

  virtual bool equals(const Module *other) const {
    if (other->get_type() != type) {
      return false;
    }

    const If *other_cast = static_cast<const If *>(other);

    if (!kutil::solver_toolbox.are_exprs_always_equal(
            condition, other_cast->get_condition())) {
      return false;
    }

    return true;
  }

  const klee::ref<klee::Expr> &get_condition() const { return condition; }
};

class IfGenerator : public x86ModuleGenerator {
public:
  IfGenerator() : x86ModuleGenerator(ModuleType::x86_If) {}

protected:
  virtual modgen_report_t process_node(const EP *ep,
                                       const bdd::Node *node) const override {
    if (node->get_type() != bdd::NodeType::BRANCH) {
      return modgen_report_t();
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
    new_ep->process_leaf(if_node, {then_leaf, else_leaf});

    return modgen_report_t(if_module, {new_ep});
  }
};

} // namespace x86
} // namespace synapse
