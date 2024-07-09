#pragma once

#include "tofino_cpu_module.h"

namespace synapse {
namespace tofino_cpu {

class ChecksumUpdate : public TofinoCPUModule {
private:
  addr_t ip_hdr_addr;
  addr_t l4_hdr_addr;
  symbol_t checksum;

public:
  ChecksumUpdate(const bdd::Node *node, addr_t _ip_hdr_addr,
                 addr_t _l4_hdr_addr, symbol_t _checksum)
      : TofinoCPUModule(ModuleType::TofinoCPU_ChecksumUpdate, "SetIpChecksum",
                        node),
        ip_hdr_addr(_ip_hdr_addr), l4_hdr_addr(_l4_hdr_addr),
        checksum(_checksum) {}

  virtual void visit(EPVisitor &visitor, const EP *ep,
                     const EPNode *ep_node) const override {
    visitor.visit(ep, ep_node, this);
  }

  virtual Module *clone() const override {
    Module *cloned =
        new ChecksumUpdate(node, ip_hdr_addr, l4_hdr_addr, checksum);
    return cloned;
  }

  addr_t get_ip_hdr_addr() const { return ip_hdr_addr; }
  addr_t get_l4_hdr_addr() const { return l4_hdr_addr; }
  const symbol_t &get_checksum() const { return checksum; }
};

class ChecksumUpdateGenerator : public TofinoCPUModuleGenerator {
public:
  ChecksumUpdateGenerator()
      : TofinoCPUModuleGenerator(ModuleType::TofinoCPU_ChecksumUpdate,
                                 "ChecksumUpdate") {}

protected:
  virtual std::optional<speculation_t>
  speculate(const EP *ep, const bdd::Node *node,
            const Context &ctx) const override {
    if (node->get_type() != bdd::NodeType::CALL) {
      return std::nullopt;
    }

    const bdd::Call *call_node = static_cast<const bdd::Call *>(node);
    const call_t &call = call_node->get_call();

    if (call.function_name != "nf_set_rte_ipv4_udptcp_checksum") {
      return std::nullopt;
    }

    return ctx;
  }

  virtual std::vector<__generator_product_t>
  process_node(const EP *ep, const bdd::Node *node) const override {
    std::vector<__generator_product_t> products;

    if (node->get_type() != bdd::NodeType::CALL) {
      return products;
    }

    const bdd::Call *call_node = static_cast<const bdd::Call *>(node);
    const call_t &call = call_node->get_call();

    if (call.function_name != "nf_set_rte_ipv4_udptcp_checksum") {
      return products;
    }

    klee::ref<klee::Expr> ip_hdr_addr_expr = call.args.at("ip_header").expr;
    klee::ref<klee::Expr> l4_hdr_addr_expr = call.args.at("l4_header").expr;
    klee::ref<klee::Expr> p = call.args.at("packet").expr;

    symbols_t symbols = call_node->get_locally_generated_symbols();
    symbol_t checksum;
    bool found = get_symbol(symbols, "checksum", checksum);
    assert(found && "Symbol checksum not found");

    addr_t ip_hdr_addr = kutil::expr_addr_to_obj_addr(ip_hdr_addr_expr);
    addr_t l4_hdr_addr = kutil::expr_addr_to_obj_addr(l4_hdr_addr_expr);

    EP *new_ep = new EP(*ep);
    products.emplace_back(new_ep);

    Module *module =
        new ChecksumUpdate(node, ip_hdr_addr, l4_hdr_addr, checksum);
    EPNode *ep_node = new EPNode(module);

    EPLeaf leaf(ep_node, node->get_next());
    new_ep->process_leaf(ep_node, {leaf});

    return products;
  }
};

} // namespace tofino_cpu
} // namespace synapse
