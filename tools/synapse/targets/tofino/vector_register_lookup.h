#pragma once

#include "tofino_module.h"

namespace synapse {
namespace tofino {

class VectorRegisterLookup : public TofinoModule {
private:
  std::unordered_set<DS_ID> rids;
  addr_t obj;

public:
  VectorRegisterLookup(const bdd::Node *node,
                       const std::unordered_set<DS_ID> &_rids, addr_t _obj)
      : TofinoModule(ModuleType::Tofino_VectorRegisterLookup,
                     "VectorRegisterLookup", node),
        rids(_rids), obj(_obj) {}

  virtual void visit(EPVisitor &visitor, const EP *ep,
                     const EPNode *ep_node) const override {
    visitor.visit(ep, ep_node, this);
  }

  virtual Module *clone() const override {
    Module *cloned = new VectorRegisterLookup(node, rids, obj);
    return cloned;
  }

  const std::unordered_set<DS_ID> &get_rids() const { return rids; }
  addr_t get_obj() const { return obj; }

  virtual std::unordered_set<DS_ID> get_generated_ds() const override {
    return rids;
  }
};

class VectorRegisterLookupGenerator : public TofinoModuleGenerator {
public:
  VectorRegisterLookupGenerator()
      : TofinoModuleGenerator(ModuleType::Tofino_VectorRegisterLookup,
                              "VectorRegisterLookup") {}

protected:
  virtual std::vector<const EP *>
  process_node(const EP *ep, const bdd::Node *node) const override {
    std::vector<const EP *> new_eps;

    if (node->get_type() != bdd::NodeType::CALL) {
      return new_eps;
    }

    const bdd::Call *call_node = static_cast<const bdd::Call *>(node);
    const call_t &call = call_node->get_call();

    if (call.function_name != "vector_borrow") {
      return new_eps;
    }

    if (!is_vector_read(call_node)) {
      return new_eps;
    }

    if (!can_place(ep, call_node, "vector",
                   PlacementDecision::TofinoRegister)) {
      return new_eps;
    }

    klee::ref<klee::Expr> vector_addr_expr = call.args.at("vector").expr;
    klee::ref<klee::Expr> index = call.args.at("index").expr;
    klee::ref<klee::Expr> value = call.extra_vars.at("borrowed_cell").second;

    addr_t vector_addr = kutil::expr_addr_to_obj_addr(vector_addr_expr);

    const Context &ctx = ep->get_ctx();
    const bdd::vector_config_t &cfg = ctx.get_vector_config(vector_addr);

    int num_entries = cfg.capacity;
    int index_size = index->getWidth();
    std::unordered_set<DS_ID> rids;

    std::vector<DS *> regs =
        build_registers(ep, node, num_entries, index_size, value, rids);

    const TofinoContext *tofino_ctx = get_tofino_ctx(ep);
    std::unordered_set<DS_ID> deps = tofino_ctx->get_stateful_deps(ep);

    if (!tofino_ctx->check_many_placements(ep, {regs}, deps)) {
      for (DS *reg : regs) {
        delete reg;
      }
      return new_eps;
    }

    Module *module = new VectorRegisterLookup(node, rids, vector_addr);
    EPNode *ep_node = new EPNode(module);

    EP *new_ep = new EP(*ep);
    new_eps.push_back(new_ep);

    EPLeaf leaf(ep_node, node->get_next());
    new_ep->process_leaf(ep_node, {leaf});

    place_regs(new_ep, vector_addr, regs, deps);

    return new_eps;
  }

private:
  void place_regs(EP *ep, addr_t obj, const std::vector<DS *> &regs,
                  const std::unordered_set<DS_ID> &deps) const {
    TofinoContext *tofino_ctx = get_mutable_tofino_ctx(ep);
    place(ep, obj, PlacementDecision::TofinoRegister);
    tofino_ctx->place_many(ep, obj, {regs}, deps);
  }

  std::vector<DS *> build_registers(const EP *ep, const bdd::Node *node,
                                    int num_entries, int index_size,
                                    klee::ref<klee::Expr> value,
                                    std::unordered_set<DS_ID> &rids) const {
    std::vector<DS *> registers;

    const TNA &tna = get_tna(ep);
    const TNAConstraints &tna_constr = tna.get_constraints();

    std::vector<klee::ref<klee::Expr>> partitions =
        partition_value(tna_constr, value);

    for (klee::ref<klee::Expr> partition : partitions) {
      DS_ID rid = "vector_" + std::to_string(node->get_id()) + "_" +
                  std::to_string(rids.size());
      Register *reg =
          new Register(tna_constr, rid, num_entries, 1, index_size, partition);
      registers.push_back(reg);
      rids.insert(rid);
    }

    return registers;
  }

  std::vector<klee::ref<klee::Expr>>
  partition_value(const TNAConstraints &tna_constr,
                  klee::ref<klee::Expr> value) const {
    std::vector<klee::ref<klee::Expr>> partitions;

    bits_t value_width = value->getWidth();
    bits_t partition_width = tna_constr.max_salu_size;

    bits_t offset = 0;
    while (offset < value_width) {
      if (offset + partition_width > value_width) {
        partition_width = value_width - offset;
      }

      klee::ref<klee::Expr> partition =
          kutil::solver_toolbox.exprBuilder->Extract(value, offset,
                                                     partition_width);
      partitions.push_back(partition);

      offset += partition_width;
    }

    return partitions;
  }
};

} // namespace tofino
} // namespace synapse
