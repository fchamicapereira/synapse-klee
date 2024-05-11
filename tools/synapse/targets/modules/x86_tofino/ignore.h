#pragma once

#include "../module.h"
#include "memory_bank.h"

namespace synapse {
namespace targets {
namespace x86_tofino {

class Ignore : public Module {
private:
  std::vector<std::string> functions_to_ignore;

public:
  Ignore()
      : Module(ModuleType::x86_Tofino_Ignore, TargetType::x86_Tofino,
               "Ignore") {
    functions_to_ignore = std::vector<std::string>{
        "current_time",
        "expire_items_single_map",
        "rte_ether_addr_hash",
    };
  }

  Ignore(bdd::Node_ptr node)
      : Module(ModuleType::x86_Tofino_Ignore, TargetType::x86_Tofino, "Ignore",
               node) {}

private:
  void process_current_time(const ExecutionPlan &ep, const bdd::Call *casted) {
    auto call = casted->get_call();
    assert(!call.ret.isNull());

    auto _time = call.ret;
    auto _generated_symbols = casted->get_locally_generated_symbols();
    assert(_generated_symbols.size() == 1);

    auto time_symbol = *_generated_symbols.begin();

    auto mb = ep.get_memory_bank<x86TofinoMemoryBank>(x86_Tofino);
    mb->add_time(time_symbol);
  }

  void process_expire_items_single_map(const ExecutionPlan &ep,
                                       const bdd::Call *casted) {
    auto call = casted->get_call();

    assert(!call.args["chain"].expr.isNull());
    assert(!call.args["map"].expr.isNull());
    assert(!call.args["vector"].expr.isNull());
    assert(!call.args["time"].expr.isNull());

    auto _chain = call.args["chain"].expr;
    auto _map = call.args["map"].expr;
    auto _vector = call.args["vector"].expr;
    auto _time = call.args["time"].expr;

    auto _chain_addr = kutil::expr_addr_to_obj_addr(_chain);
    auto _map_addr = kutil::expr_addr_to_obj_addr(_map);
    auto _vector_addr = kutil::expr_addr_to_obj_addr(_vector);

    auto expiration = x86TofinoMemoryBank::expiration_t(_chain_addr, _map_addr,
                                                        _vector_addr, _time);

    auto mb = ep.get_memory_bank<x86TofinoMemoryBank>(x86_Tofino);
    mb->set_expiration(expiration);
  }

  processing_result_t process(const ExecutionPlan &ep,
                              bdd::Node_ptr node) override {
    processing_result_t result;

    auto casted = bdd::cast_node<bdd::Call>(node);

    if (!casted) {
      return result;
    }

    auto call = casted->get_call();

    auto found_it = std::find(functions_to_ignore.begin(),
                              functions_to_ignore.end(), call.function_name);

    if (call.function_name == "current_time") {
      process_current_time(ep, casted);
    } else if (call.function_name == "expire_items_single_map") {
      process_expire_items_single_map(ep, casted);
    }

    if (found_it != functions_to_ignore.end()) {
      auto new_module = std::make_shared<Ignore>(node);
      auto new_ep = ep.ignore_leaf(node->get_next(), TargetType::x86_Tofino);

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
} // namespace x86_tofino
} // namespace targets
} // namespace synapse
