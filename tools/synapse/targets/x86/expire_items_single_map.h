#pragma once

#include "x86_module.h"

namespace synapse {
namespace x86 {

class ExpireItemsSingleMap : public x86Module {
private:
  addr_t dchain_addr;
  addr_t vector_addr;
  addr_t map_addr;
  klee::ref<klee::Expr> time;
  klee::ref<klee::Expr> total_freed;

public:
  ExpireItemsSingleMap(const bdd::Node *node, addr_t _dchain_addr,
                       addr_t _vector_addr, addr_t _map_addr,
                       klee::ref<klee::Expr> _time,
                       klee::ref<klee::Expr> _total_freed)
      : x86Module(ModuleType::x86_ExpireItemsSingleMap, "ExpireItemsSingleMap",
                  node),
        dchain_addr(_dchain_addr), vector_addr(_vector_addr),
        map_addr(_map_addr), time(_time), total_freed(_total_freed) {}

  virtual void visit(EPVisitor &visitor, const EP *ep,
                     const EPNode *ep_node) const override {
    visitor.visit(ep, ep_node, this);
  }

  virtual Module *clone() const {
    ExpireItemsSingleMap *cloned = new ExpireItemsSingleMap(
        node, dchain_addr, map_addr, vector_addr, time, total_freed);
    return cloned;
  }

  addr_t get_dchain_addr() const { return dchain_addr; }
  addr_t get_vector_addr() const { return vector_addr; }
  addr_t get_map_addr() const { return map_addr; }
  klee::ref<klee::Expr> get_time() const { return time; }
  klee::ref<klee::Expr> get_total_freed() const { return total_freed; }
};

class ExpireItemsSingleMapGenerator : public x86ModuleGenerator {
public:
  ExpireItemsSingleMapGenerator()
      : x86ModuleGenerator(ModuleType::x86_ExpireItemsSingleMap,
                           "ExpireItemsSingleMap") {}

protected:
  virtual std::vector<const EP *>
  process_node(const EP *ep, const bdd::Node *node) const override {
    std::vector<const EP *> new_eps;

    if (node->get_type() != bdd::NodeType::CALL) {
      return new_eps;
    }

    const bdd::Call *call_node = static_cast<const bdd::Call *>(node);
    const call_t &call = call_node->get_call();

    if (call.function_name != "expire_items_single_map") {
      return new_eps;
    }

    klee::ref<klee::Expr> dchain = call.args.at("chain").expr;
    klee::ref<klee::Expr> vector = call.args.at("vector").expr;
    klee::ref<klee::Expr> map = call.args.at("map").expr;
    klee::ref<klee::Expr> time = call.args.at("time").expr;
    klee::ref<klee::Expr> total_freed = call.ret;

    addr_t map_addr = kutil::expr_addr_to_obj_addr(map);
    addr_t vector_addr = kutil::expr_addr_to_obj_addr(vector);
    addr_t dchain_addr = kutil::expr_addr_to_obj_addr(dchain);

    Module *module = new ExpireItemsSingleMap(node, dchain_addr, vector_addr,
                                              map_addr, time, total_freed);
    EPNode *ep_node = new EPNode(module);

    EP *new_ep = new EP(*ep);
    new_eps.push_back(new_ep);

    if (node->get_next()) {
      EPLeaf leaf(ep_node, node->get_next());
      new_ep->process_leaf(ep_node, {leaf});
    } else {
      new_ep->process_leaf(ep_node, {});
    }

    return new_eps;
  }
};

} // namespace x86
} // namespace synapse
