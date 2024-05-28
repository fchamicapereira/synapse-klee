#pragma once

#include "x86_module.h"

namespace synapse {
namespace x86 {

class PacketReturnChunk : public x86Module {
private:
  addr_t chunk_addr;
  klee::ref<klee::Expr> original_chunk;
  std::vector<modification_t> modifications;

public:
  PacketReturnChunk()
      : x86Module(ModuleType::x86_PacketReturnChunk, "PacketReturnChunk") {}

  PacketReturnChunk(const bdd::Node *node, addr_t _chunk_addr,
                    klee::ref<klee::Expr> _original_chunk,
                    const std::vector<modification_t> &_modifications)
      : x86Module(ModuleType::x86_PacketReturnChunk, "PacketReturnChunk", node),
        chunk_addr(_chunk_addr), original_chunk(_original_chunk),
        modifications(_modifications) {}

private:
  klee::ref<klee::Expr> get_original_chunk(const EP *ep,
                                           const bdd::Node *node) const {
    auto prev_borrows = get_prev_fn(ep, node, "packet_borrow_next_chunk");
    auto prev_returns = get_prev_fn(ep, node, "packet_return_chunk");

    assert(prev_borrows.size());
    assert(prev_borrows.size() > prev_returns.size());

    auto target = prev_borrows[prev_returns.size()];

    auto call_node = static_cast<const bdd::Call *>(target);
    assert(call_node);

    auto call = call_node->get_call();

    assert(call.function_name == "packet_borrow_next_chunk");
    assert(!call.extra_vars["the_chunk"].second.isNull());

    return call.extra_vars["the_chunk"].second;
  }

  generated_data_t process(const EP *ep, const bdd::Node *node) override {
    generated_data_t result;

    auto casted = static_cast<const bdd::Call *>(node);

    if (!casted) {
      return result;
    }

    auto call = casted->get_call();

    if (call.function_name != "packet_return_chunk") {
      return result;
    }

    assert(!call.args["the_chunk"].expr.isNull());
    assert(!call.args["the_chunk"].in.isNull());

    auto _chunk = call.args["the_chunk"].expr;
    auto _current_chunk = call.args["the_chunk"].in;
    auto _original_chunk = get_original_chunk(ep, node);

    auto _chunk_addr = kutil::expr_addr_to_obj_addr(_chunk);
    auto _modifications = build_modifications(_original_chunk, _current_chunk);

    auto new_module = std::make_shared<PacketReturnChunk>(
        node, _chunk_addr, _original_chunk, _modifications);
    auto new_ep = ep.process_leaf(new_module, node->get_next());

    result.module = new_module;
    result.next_eps.push_back(new_ep);

    return result;
  }

public:
  virtual void visit(EPVisitor &visitor, const EPNode *ep_node) const override {
    visitor.visit(ep_node, this);
  }

  virtual Module_ptr clone() const override {
    auto cloned =
        new PacketReturnChunk(node, chunk_addr, original_chunk, modifications);
    return std::shared_ptr<Module>(cloned);
  }

  virtual bool equals(const Module *other) const override {
    if (other->get_type() != type) {
      return false;
    }

    auto other_cast = static_cast<const PacketReturnChunk *>(other);

    if (chunk_addr != other_cast->get_chunk_addr()) {
      return false;
    }

    if (!kutil::solver_toolbox.are_exprs_always_equal(
            original_chunk, other_cast->original_chunk)) {
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

  const addr_t &get_chunk_addr() const { return chunk_addr; }

  klee::ref<klee::Expr> get_original_chunk() const { return original_chunk; }

  const std::vector<modification_t> &get_modifications() const {
    return modifications;
  }
};

} // namespace x86
} // namespace synapse
