#pragma once

#include "ignore.h"
#include "tofino_module.h"

namespace synapse {
namespace targets {
namespace tofino {

class SetupExpirationNotifications : public TofinoModule {
public:
  SetupExpirationNotifications()
      : TofinoModule(ModuleType::Tofino_SetupExpirationNotifications,
                     "SetupExpirationNotifications") {}

  SetupExpirationNotifications(bdd::Node_ptr node)
      : TofinoModule(ModuleType::Tofino_SetupExpirationNotifications,
                     "SetupExpirationNotifications", node) {}

private:
  time_ns_t get_expiration_time(klee::ref<klee::Expr> expr) const {
    auto symbol = kutil::get_symbol(expr);
    assert(symbol.first);
    auto time_symbol = symbol.second;

    auto time = kutil::solver_toolbox.create_new_symbol(time_symbol, 64);
    auto zero = kutil::solver_toolbox.exprBuilder->Constant(0, 64);
    auto time_eq_0 = kutil::solver_toolbox.exprBuilder->Eq(time, zero);

    klee::ConstraintManager constraints;
    constraints.addConstraint(time_eq_0);

    auto value =
        kutil::solver_toolbox.signed_value_from_expr(expr, constraints);

    return value * -1;
  }

  void remember_expiration_data(const ExecutionPlan &ep,
                                klee::ref<klee::Expr> time,
                                const bdd::symbol_t &number_of_freed_flows) {
    auto expiration_time = get_expiration_time(time);
    expiration_data_t expiration_data(expiration_time, number_of_freed_flows);

    auto mb = ep.get_memory_bank();
    mb->set_expiration_data(expiration_data);
  }

  processing_result_t process(const ExecutionPlan &ep,
                              bdd::Node_ptr node) override {
    processing_result_t result;

    auto casted = bdd::cast_node<bdd::Call>(node);

    if (!casted) {
      return result;
    }

    auto call = casted->get_call();

    if (call.function_name != "expire_items_single_map") {
      return result;
    }

    assert(!call.args["chain"].expr.isNull());
    assert(!call.args["vector"].expr.isNull());
    assert(!call.args["map"].expr.isNull());
    assert(!call.args["time"].expr.isNull());
    assert(!call.ret.isNull());

    auto _dchain_addr = call.args["chain"].expr;
    auto _vector_addr = call.args["vector"].expr;
    auto _map_addr = call.args["map"].expr;
    auto _time = call.args["time"].expr;
    auto _generated_symbols = casted->get_locally_generated_symbols();

    assert(_generated_symbols.size() == 1);
    auto _number_of_freed_flows = *_generated_symbols.begin();

    remember_expiration_data(ep, _time, _number_of_freed_flows);

    return ignore(ep, node);
  }

public:
  virtual void visit(ExecutionPlanVisitor &visitor,
                     const ExecutionPlanNode *ep_node) const override {
    visitor.visit(ep_node, this);
  }

  virtual Module_ptr clone() const override {
    auto cloned = new SetupExpirationNotifications(node);
    return std::shared_ptr<Module>(cloned);
  }

  virtual bool equals(const Module *other) const override {
    if (other->get_type() != type) {
      return false;
    }

    return true;
  }
};
} // namespace tofino
} // namespace targets
} // namespace synapse
