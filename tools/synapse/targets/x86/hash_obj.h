#pragma once

#include "x86_module.h"

namespace synapse {
namespace x86 {

class HashObj : public x86Module {
private:
  addr_t obj_addr;
  klee::ref<klee::Expr> size;
  klee::ref<klee::Expr> hash;

public:
  HashObj() : x86Module(ModuleType::x86_HashObj, "HashObj") {}

  HashObj(const bdd::Node *node, addr_t _obj_addr, klee::ref<klee::Expr> _size,
          klee::ref<klee::Expr> _hash)
      : x86Module(ModuleType::x86_HashObj, "HashObj", node),
        obj_addr(_obj_addr), size(_size), hash(_hash) {}

private:
  generated_data_t process(const EP *ep, const bdd::Node *node) override {
    generated_data_t result;

    auto casted = static_cast<const bdd::Call *>(node);

    if (!casted) {
      return result;
    }

    auto call = casted->get_call();

    if (call.function_name == "hash_obj") {
      assert(!call.args["obj"].expr.isNull());
      assert(!call.args["size"].expr.isNull());
      assert(!call.ret.isNull());

      auto _obj = call.args["obj"].expr;
      auto _size = call.args["size"].expr;
      auto _hash = call.ret;

      auto _obj_addr = kutil::expr_addr_to_obj_addr(_obj);

      auto new_module =
          std::make_shared<HashObj>(node, _obj_addr, _size, _hash);
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
    auto cloned = new HashObj(node, obj_addr, size, hash);
    return std::shared_ptr<Module>(cloned);
  }

  virtual bool equals(const Module *other) const override {
    if (other->get_type() != type) {
      return false;
    }

    auto other_cast = static_cast<const HashObj *>(other);

    if (obj_addr != other_cast->get_obj_addr()) {
      return false;
    }

    if (!kutil::solver_toolbox.are_exprs_always_equal(size,
                                                      other_cast->get_size())) {
      return false;
    }

    if (!kutil::solver_toolbox.are_exprs_always_equal(hash,
                                                      other_cast->get_hash())) {
      return false;
    }

    return true;
  }

  addr_t get_obj_addr() const { return obj_addr; }
  klee::ref<klee::Expr> get_size() const { return size; }
  klee::ref<klee::Expr> get_hash() const { return hash; }
};

} // namespace x86
} // namespace synapse
