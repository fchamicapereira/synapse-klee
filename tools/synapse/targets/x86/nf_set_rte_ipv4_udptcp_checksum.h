#pragma once

#include "x86_module.h"

namespace synapse {
namespace x86 {

class SetIpv4UdpTcpChecksum : public x86Module {
private:
  addr_t ip_header_addr;
  addr_t l4_header_addr;
  symbol_t checksum;

public:
  SetIpv4UdpTcpChecksum()
      : x86Module(ModuleType::x86_SetIpv4UdpTcpChecksum, "SetIpChecksum") {}

  SetIpv4UdpTcpChecksum(const bdd::Node *node, addr_t _ip_header_addr,
                        addr_t _l4_header_addr, symbol_t _checksum)
      : x86Module(ModuleType::x86_SetIpv4UdpTcpChecksum, "SetIpChecksum", node),
        ip_header_addr(_ip_header_addr), l4_header_addr(_l4_header_addr),
        checksum(_checksum) {}

private:
  generated_data_t process(const EP *ep, const bdd::Node *node) override {
    generated_data_t result;

    auto casted = static_cast<const bdd::Call *>(node);

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
      symbol_t _checksum;
      auto success = get_symbol(_generated_symbols, "checksum", _checksum);
      assert(success && "Symbol checksum not found");

      auto _ip_header_addr = kutil::expr_addr_to_obj_addr(_ip_header);
      auto _l4_header_addr = kutil::expr_addr_to_obj_addr(_l4_header);

      auto new_module = std::make_shared<SetIpv4UdpTcpChecksum>(
          node, _ip_header_addr, _l4_header_addr, _checksum);
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

    if (checksum.array->name != other_cast->get_checksum().array->name) {
      return false;
    }

    return true;
  }

  addr_t get_ip_header_addr() const { return ip_header_addr; }
  addr_t get_l4_header_addr() const { return l4_header_addr; }
  const symbol_t &get_checksum() const { return checksum; }
};

} // namespace x86
} // namespace synapse
