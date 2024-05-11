#pragma once

#include "x86_module.h"

namespace synapse {
namespace targets {
namespace x86 {

class SetIpv4UdpTcpChecksum : public x86Module {
private:
  addr_t ip_header_addr;
  addr_t l4_header_addr;
  bdd::symbol_t checksum;

public:
  SetIpv4UdpTcpChecksum()
      : x86Module(ModuleType::x86_SetIpv4UdpTcpChecksum, "SetIpChecksum") {}

  SetIpv4UdpTcpChecksum(bdd::Node_ptr node, addr_t _ip_header_addr,
                        addr_t _l4_header_addr, bdd::symbol_t _checksum)
      : x86Module(ModuleType::x86_SetIpv4UdpTcpChecksum, "SetIpChecksum", node),
        ip_header_addr(_ip_header_addr), l4_header_addr(_l4_header_addr),
        checksum(_checksum) {}

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

      auto _ip_header = call.args["ip_header"].expr;
      auto _l4_header = call.args["l4_header"].expr;
      auto _p = call.args["packet"].expr;

      auto _generated_symbols = casted->get_locally_generated_symbols();
      auto _checksum = bdd::get_symbol(_generated_symbols, "checksum");

      auto _ip_header_addr = kutil::expr_addr_to_obj_addr(_ip_header);
      auto _l4_header_addr = kutil::expr_addr_to_obj_addr(_l4_header);

      auto new_module = std::make_shared<SetIpv4UdpTcpChecksum>(
          node, _ip_header_addr, _l4_header_addr, _checksum);
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
    auto cloned = new SetIpv4UdpTcpChecksum(node, ip_header_addr,
                                            l4_header_addr, checksum);
    return std::shared_ptr<Module>(cloned);
  }

  virtual bool equals(const Module *other) const override {
    if (other->get_type() != type) {
      return false;
    }

    auto other_cast = static_cast<const SetIpv4UdpTcpChecksum *>(other);

    if (ip_header_addr != other_cast->get_ip_header_addr()) {
      return false;
    }

    if (l4_header_addr != other_cast->get_l4_header_addr()) {
      return false;
    }

    if (checksum.label != other_cast->get_checksum().label) {
      return false;
    }

    return true;
  }

  addr_t get_ip_header_addr() const { return ip_header_addr; }
  addr_t get_l4_header_addr() const { return l4_header_addr; }
  const bdd::symbol_t &get_checksum() const { return checksum; }
};
} // namespace x86
} // namespace targets
} // namespace synapse
