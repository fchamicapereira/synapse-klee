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
  If() : x86Module(ModuleType::x86_If, "If") {}

  If(const bdd::Node *node, klee::ref<klee::Expr> _condition)
      : x86Module(ModuleType::x86_If, "If", node), condition(_condition) {}

private:
  generated_data_t process(const EP &ep, const bdd::Node *node) override {
    generated_data_t result;

    auto casted = static_cast<const bdd::Branch *>(node);

    if (!casted) {
      return result;
    }

    assert(!casted->get_condition().isNull());
    auto _condition = casted->get_condition();

    auto new_if_module = std::make_shared<If>(node, _condition);
    auto new_then_module = std::make_shared<Then>(node);
    auto new_else_module = std::make_shared<Else>(node);

    auto if_leaf = EPLeaf(new_if_module, nullptr);
    auto then_leaf = EPLeaf(new_then_module, casted->get_on_true());
    auto else_leaf = EPLeaf(new_else_module, casted->get_on_false());

    std::vector<EPLeaf> if_leaves{if_leaf};
    std::vector<EPLeaf> then_else_leaves{then_leaf, else_leaf};

    auto ep_if = ep.process_leaf(if_leaves);
    auto ep_if_then_else = ep_if.process_leaf(then_else_leaves);

    result.module = new_if_module;
    result.next_eps.push_back(ep_if_then_else);

    return result;
  }

public:
  virtual void visit(EPVisitor &visitor, const EPNode *ep_node) const override {
    visitor.visit(ep_node, this);
  }

  virtual Module_ptr clone() const override {
    auto cloned = new If(node, condition);
    return std::shared_ptr<Module>(cloned);
  }

  virtual bool equals(const Module *other) const override {
    if (other->get_type() != type) {
      return false;
    }

    auto other_cast = static_cast<const If *>(other);

    if (!kutil::solver_toolbox.are_exprs_always_equal(
            condition, other_cast->get_condition())) {
      return false;
    }

    return true;
  }

  const klee::ref<klee::Expr> &get_condition() const { return condition; }
};

} // namespace x86
} // namespace synapse
