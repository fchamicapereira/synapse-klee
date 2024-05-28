#pragma once

#include "x86_module.h"

namespace synapse {
namespace x86 {

class VectorReturn : public x86Module {
private:
  addr_t vector_addr;
  klee::ref<klee::Expr> index;
  addr_t value_addr;
  std::vector<modification_t> modifications;

public:
  VectorReturn() : x86Module(ModuleType::x86_VectorReturn, "VectorReturn") {}

  VectorReturn(const bdd::Node *node, addr_t _vector_addr,
               klee::ref<klee::Expr> _index, addr_t _value_addr,
               const std::vector<modification_t> &_modifications)
      : x86Module(ModuleType::x86_VectorReturn, "VectorReturn", node),
        vector_addr(_vector_addr), index(_index), value_addr(_value_addr),
        modifications(_modifications) {}

private:
  generated_data_t process(const EP *ep, const bdd::Node *node) override {
    generated_data_t result;

    auto casted = static_cast<const bdd::Call *>(node);

    if (!casted) {
      return result;
    }

    auto call = casted->get_call();

    if (call.function_name == "vector_return") {
      assert(!call.args["vector"].expr.isNull());
      assert(!call.args["index"].expr.isNull());
      assert(!call.args["value"].expr.isNull());
      assert(!call.args["value"].in.isNull());

      auto _vector = call.args["vector"].expr;
      auto _index = call.args["index"].expr;
      auto _value_addr_expr = call.args["value"].expr;
      auto _value = call.args["value"].in;

      auto _vector_addr = kutil::expr_addr_to_obj_addr(_vector);
      auto _value_addr = kutil::expr_addr_to_obj_addr(_value_addr_expr);

      auto _original_value = get_original_vector_value(ep, node, _vector_addr);
      auto _modifications = build_modifications(_original_value, _value);

      save_vector(ep, _vector_addr);

      auto new_module = std::make_shared<VectorReturn>(
          node, _vector_addr, _index, _value_addr, _modifications);
      auto new_ep = ep.process_leaf(new_module, node->get_next());

      result.module = new_module;
      result.next_eps.push_back(new_ep);
    }

    return result;
  }

public:
  virtual void visit(EPVisitor &visitor, const EPNode *ep_node) const override {
    visitor.visit(ep_node, this);
  }

  virtual Module_ptr clone() const override {
    auto cloned =
        new VectorReturn(node, vector_addr, index, value_addr, modifications);
    return std::shared_ptr<Module>(cloned);
  }

  virtual bool equals(const Module *other) const override {
    if (other->get_type() != type) {
      return false;
    }

    auto other_cast = static_cast<const VectorReturn *>(other);

    if (vector_addr != other_cast->get_vector_addr()) {
      return false;
    }

    if (!kutil::solver_toolbox.are_exprs_always_equal(
            index, other_cast->get_index())) {
      return false;
    }

    if (value_addr != other_cast->get_value_addr()) {
      return false;
    }

    auto other_modifications = other_cast->get_modifications();

    if (modifications.size() != other_modifications.size()) {
      return false;
    }

    for (unsigned i = 0; i < modifications.size(); i++) {
      auto modification = modifications[i];
      auto other_modification = other_modifications[i];

      if (modification.byte != other_modification.byte) {
        return false;
      }

      if (!kutil::solver_toolbox.are_exprs_always_equal(
              modification.expr, other_modification.expr)) {
        return false;
      }
    }

    return true;
  }

  addr_t get_vector_addr() const { return vector_addr; }
  klee::ref<klee::Expr> get_index() const { return index; }
  addr_t get_value_addr() const { return value_addr; }

  const std::vector<modification_t> &get_modifications() const {
    return modifications;
  }
};

} // namespace x86
} // namespace synapse
