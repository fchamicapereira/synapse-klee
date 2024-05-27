#pragma once

#include "x86_module.h"

namespace synapse {
namespace x86 {

class PacketBorrowNextChunk : public x86Module {
private:
  addr_t p_addr;
  addr_t chunk_addr;
  klee::ref<klee::Expr> chunk;
  klee::ref<klee::Expr> length;

public:
  PacketBorrowNextChunk(const bdd::Node *node, addr_t _p_addr,
                        addr_t _chunk_addr, klee::ref<klee::Expr> _chunk,
                        klee::ref<klee::Expr> _length)
      : x86Module(ModuleType::x86_PacketBorrowNextChunk,
                  "PacketBorrowNextChunk", node),
        p_addr(_p_addr), chunk_addr(_chunk_addr), chunk(_chunk),
        length(_length) {}

public:
  virtual void visit(EPVisitor &visitor, const EPNode *ep_node) const override {
    visitor.visit(ep_node, this);
  }

  virtual Module *clone() const override {
    return new PacketBorrowNextChunk(node, p_addr, chunk_addr, chunk, length);
  }

  virtual bool equals(const Module *other) const override {
    if (other->get_type() != type) {
      return false;
    }

    auto other_cast = static_cast<const PacketBorrowNextChunk *>(other);

    if (p_addr != other_cast->get_p_addr()) {
      return false;
    }

    if (chunk_addr != other_cast->get_chunk_addr()) {
      return false;
    }

    if (!kutil::solver_toolbox.are_exprs_always_equal(
            chunk, other_cast->get_chunk())) {
      return false;
    }

    if (!kutil::solver_toolbox.are_exprs_always_equal(
            length, other_cast->get_length())) {
      return false;
    }

    return true;
  }

  addr_t get_p_addr() const { return p_addr; }
  addr_t get_chunk_addr() const { return chunk_addr; }
  klee::ref<klee::Expr> get_chunk() const { return chunk; }
  klee::ref<klee::Expr> get_length() const { return length; }
};

class PacketBorrowNextChunkGenerator : public x86ModuleGenerator {
protected:
  ModuleType type;
  TargetType target;

public:
  PacketBorrowNextChunkGenerator()
      : x86ModuleGenerator(ModuleType::x86_Broadcast) {}

protected:
  generated_data_t process_node(const EP &ep, const bdd::Node *node) override {
    generated_data_t result;

    if (node->get_type() != bdd::NodeType::CALL) {
      return result;
    }

    const bdd::Call *casted = static_cast<const bdd::Call *>(node);
    const call_t &call = casted->get_call();

    if (call.function_name != "packet_borrow_next_chunk") {
      return result;
    }

    klee::ref<klee::Expr> _p = call.args.at("p").expr;
    klee::ref<klee::Expr> _chunk = call.args.at("chunk").out;
    klee::ref<klee::Expr> _out_chunk = call.extra_vars.at("the_chunk").second;
    klee::ref<klee::Expr> _length = call.args.at("length").expr;

    assert(!_p.isNull());
    assert(!_chunk.isNull());
    assert(!_out_chunk.isNull());
    assert(!_length.isNull());

    addr_t _p_addr = kutil::expr_addr_to_obj_addr(_p);
    addr_t _chunk_addr = kutil::expr_addr_to_obj_addr(_chunk);

    std::unique_ptr<Module> new_module =
        std::make_unique<PacketBorrowNextChunk>(node, _p_addr, _chunk_addr,
                                                _out_chunk, _length);
    EP new_ep = ep.process_leaf(new_module, node->get_next());

    result.module = new_module.get();
    result.next.push_back(new_ep);

    return result;
  }
};

} // namespace x86
} // namespace synapse
