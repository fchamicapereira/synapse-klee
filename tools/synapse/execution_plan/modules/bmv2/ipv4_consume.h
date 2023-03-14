#pragma once

#include "../module.h"

namespace synapse {
namespace targets {
namespace bmv2 {

class IPv4Consume : public Module {
private:
  klee::ref<klee::Expr> chunk;

public:
  IPv4Consume()
      : Module(ModuleType::BMv2_IPv4Consume, TargetType::BMv2, "IPv4Consume") {}

  IPv4Consume(BDD::BDDNode_ptr node, klee::ref<klee::Expr> _chunk)
      : Module(ModuleType::BMv2_IPv4Consume, TargetType::BMv2, "IPv4Consume", node),
        chunk(_chunk) {}

private:
  bool is_valid_ipv4(const BDD::Node *ethernet_node, klee::ref<klee::Expr> len,
                     const std::vector<klee::ConstraintManager> &constraints) {
    assert(ethernet_node);
    assert(ethernet_node->get_type() == BDD::Node::NodeType::CALL);

    auto call_node = static_cast<const BDD::Call *>(ethernet_node);
    auto call = call_node->get_call();

    auto ethernet_chunk = call.extra_vars[symbex::FN_BORROW_CHUNK_EXTRA].second;

    assert(!ethernet_chunk.isNull());
    assert(!len.isNull());

    // Make sure that packet_borrow_next_chunk borrows the next 20 bytes
    assert(len->getKind() == klee::Expr::Kind::Constant);
    if (kutil::solver_toolbox.value_from_expr(len) != 20) {
      return false;
    }

    auto eth_type_expr = kutil::solver_toolbox.exprBuilder->Extract(
        ethernet_chunk, 12 * 8, 2 * 8);
    auto eth_type_ipv4 = kutil::solver_toolbox.exprBuilder->Constant(
        UINT_16_SWAP_ENDIANNESS(0x0800), 2 * 8);
    auto eq =
        kutil::solver_toolbox.exprBuilder->Eq(eth_type_expr, eth_type_ipv4);

    kutil::RetrieveSymbols symbol_retriever;
    symbol_retriever.visit(eq);
    auto symbols = symbol_retriever.get_retrieved();
    kutil::ReplaceSymbols symbol_replacer(symbols);

    for (auto constraint : constraints) {
      if (!kutil::solver_toolbox.is_expr_always_true(constraint, eq,
                                                     symbol_replacer)) {
        return false;
      }
    }

    return true;
  }

  processing_result_t process_call(const ExecutionPlan &ep,
                                   BDD::BDDNode_ptr node,
                                   const BDD::Call *casted) override {
    processing_result_t result;
    auto call = casted->get_call();

    if (call.function_name != symbex::FN_BORROW_CHUNK) {
      return result;
    }

    // IPv4 should come after L2 Consume
    auto all_prev_packet_borrow_next_chunk =
        get_all_prev_functions(casted, symbex::FN_BORROW_CHUNK);

    if (all_prev_packet_borrow_next_chunk.size() != 1) {
      return result;
    }

    assert(!call.args[symbex::FN_BORROW_CHUNK_ARG_LEN].expr.isNull());
    assert(!call.extra_vars[symbex::FN_BORROW_CHUNK_EXTRA].second.isNull());

    auto _length = call.args[symbex::FN_BORROW_CHUNK_ARG_LEN].expr;
    auto _chunk = call.extra_vars[symbex::FN_BORROW_CHUNK_EXTRA].second;

    auto valid = is_valid_ipv4(all_prev_packet_borrow_next_chunk[0].get(),
                               _length, node->get_constraints());
    assert(valid);

    auto new_module = std::make_shared<IPv4Consume>(node, _chunk);
    auto new_ep = ep.add_leaves(new_module, node->get_next());

    result.module = new_module;
    result.next_eps.push_back(new_ep);

    return result;
  }

public:
  virtual void visit(ExecutionPlanVisitor &visitor) const override {
    visitor.visit(this);
  }

  virtual Module_ptr clone() const override {
    auto cloned = new IPv4Consume(node, chunk);
    return std::shared_ptr<Module>(cloned);
  }

  virtual bool equals(const Module *other) const override {
    return other->get_type() == type;
  }

  const klee::ref<klee::Expr> &get_chunk() const { return chunk; }
};
} // namespace bmv2
} // namespace targets
} // namespace synapse
