#pragma once

#include "../module.h"

namespace synapse {
namespace targets {
namespace x86_tofino {

class PacketParseEthernet : public Module {
private:
  klee::ref<klee::Expr> chunk;

public:
  PacketParseEthernet()
      : Module(ModuleType::x86_Tofino_PacketParseEthernet,
               TargetType::x86_Tofino, "PacketParseEthernet") {}

  PacketParseEthernet(bdd::Node_ptr node, klee::ref<klee::Expr> _chunk)
      : Module(ModuleType::x86_Tofino_PacketParseEthernet,
               TargetType::x86_Tofino, "PacketParseEthernet", node),
        chunk(_chunk) {}

private:
  processing_result_t process(const ExecutionPlan &ep,
                              bdd::Node_ptr node) override {
    processing_result_t result;

    auto casted = bdd::cast_node<bdd::Call>(node);

    if (!casted) {
      return result;
    }

    auto call = casted->get_call();

    if (call.function_name != "packet_borrow_next_chunk") {
      return result;
    }

    auto all_prev_packet_borrow_next_chunk =
        get_prev_fn(ep, node, "packet_borrow_next_chunk");

    if (all_prev_packet_borrow_next_chunk.size() != 0) {
      return result;
    }

    assert(!call.args["length"].expr.isNull());
    assert(!call.extra_vars["the_chunk"].second.isNull());

    auto _length = call.args["length"].expr;
    auto _chunk = call.extra_vars["the_chunk"].second;

    assert(kutil::solver_toolbox.value_from_expr(_length) == 14);

    auto new_module = std::make_shared<PacketParseEthernet>(node, _chunk);
    auto new_ep = ep.add_leaves(new_module, node->get_next());

    result.module = new_module;
    result.next_eps.push_back(new_ep);

    return result;
  }

public:
  virtual void visit(ExecutionPlanVisitor &visitor,
                     const ExecutionPlanNode *ep_node) const override {
    visitor.visit(ep_node, this);
  }

  virtual Module_ptr clone() const override {
    auto cloned = new PacketParseEthernet(node, chunk);
    return std::shared_ptr<Module>(cloned);
  }

  virtual bool equals(const Module *other) const override {
    return other->get_type() == type;
  }

  const klee::ref<klee::Expr> &get_chunk() const { return chunk; }
};
} // namespace x86_tofino
} // namespace targets
} // namespace synapse
