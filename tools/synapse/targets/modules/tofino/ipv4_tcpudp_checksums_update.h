#pragma once

#include "tofino_module.h"

namespace synapse {
namespace targets {
namespace tofino {

class IPv4TCPUDPChecksumsUpdate : public TofinoModule {
private:
  klee::ref<klee::Expr> ip_header_addr;
  klee::ref<klee::Expr> l4_header_addr;
  klee::ref<klee::Expr> p_addr;

  bdd::symbols_t generated_symbols;

public:
  IPv4TCPUDPChecksumsUpdate()
      : TofinoModule(ModuleType::Tofino_IPv4TCPUDPChecksumsUpdate,
                     "IPv4TCPUDPChecksumsUpdate") {}

  IPv4TCPUDPChecksumsUpdate(bdd::Node_ptr node,
                            klee::ref<klee::Expr> _ip_header_addr,
                            klee::ref<klee::Expr> _l4_header_addr,
                            klee::ref<klee::Expr> _p_addr,
                            bdd::symbols_t _generated_symbols)
      : TofinoModule(ModuleType::Tofino_IPv4TCPUDPChecksumsUpdate,
                     "IPv4TCPUDPChecksumsUpdate", node),
        ip_header_addr(_ip_header_addr), l4_header_addr(_l4_header_addr),
        p_addr(_p_addr), generated_symbols(_generated_symbols) {}

private:
  processing_result_t process(const ExecutionPlan &ep,
                              bdd::Node_ptr node) override {
    processing_result_t result;

    auto casted = bdd::cast_node<bdd::Call>(node);

    if (!casted) {
      return result;
    }

    auto call = casted->get_call();

    if (call.function_name == "nf_set_rte_ipv4_udptcp_checksum") {
      assert(!call.args["ip_header"].expr.isNull());
      assert(!call.args["l4_header"].expr.isNull());
      assert(!call.args["packet"].expr.isNull());

      auto _ip_header_addr = call.args["ip_header"].expr;
      auto _l4_header_addr = call.args["l4_header"].expr;
      auto _p_addr = call.args["packet"].expr;
      auto _generated_symbols = casted->get_locally_generated_symbols();

      auto new_module = std::make_shared<IPv4TCPUDPChecksumsUpdate>(
          node, _ip_header_addr, _l4_header_addr, _p_addr, _generated_symbols);
      auto new_ep = ep.add_leaves(new_module, node->get_next());

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
    auto cloned = new IPv4TCPUDPChecksumsUpdate(
        node, ip_header_addr, l4_header_addr, p_addr, generated_symbols);
    return std::shared_ptr<Module>(cloned);
  }

  virtual bool equals(const Module *other) const override {
    if (other->get_type() != type) {
      return false;
    }

    auto other_cast = static_cast<const IPv4TCPUDPChecksumsUpdate *>(other);

    if (!kutil::solver_toolbox.are_exprs_always_equal(
            ip_header_addr, other_cast->get_ip_header_addr())) {
      return false;
    }

    if (!kutil::solver_toolbox.are_exprs_always_equal(
            l4_header_addr, other_cast->get_l4_header_addr())) {
      return false;
    }

    if (!kutil::solver_toolbox.are_exprs_always_equal(
            p_addr, other_cast->get_p_addr())) {
      return false;
    }

    if (generated_symbols != other_cast->generated_symbols) {
      return false;
    }

    return true;
  }

  const klee::ref<klee::Expr> &get_ip_header_addr() const {
    return ip_header_addr;
  }
  const klee::ref<klee::Expr> &get_l4_header_addr() const {
    return l4_header_addr;
  }
  const klee::ref<klee::Expr> &get_p_addr() const { return p_addr; }

  const bdd::symbols_t &get_generated_symbols() const {
    return generated_symbols;
  }
};
} // namespace tofino
} // namespace targets
} // namespace synapse
