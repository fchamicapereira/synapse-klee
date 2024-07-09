#pragma once

#include "x86_module.h"

namespace synapse {
namespace x86 {

class ChecksumUpdate : public x86Module {
private:
  addr_t ip_hdr_addr;
  addr_t l4_hdr_addr;
  symbol_t checksum;

public:
  ChecksumUpdate(const bdd::Node *node, addr_t _ip_hdr_addr,
                 addr_t _l4_hdr_addr, symbol_t _checksum)
      : x86Module(ModuleType::x86_ChecksumUpdate, "SetIpChecksum", node),
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

class ChecksumUpdateGenerator : public x86ModuleGenerator {
public:
  ChecksumUpdateGenerator()
      : x86ModuleGenerator(ModuleType::x86_ChecksumUpdate, "ChecksumUpdate") {}

protected:
  bool bdd_node_match_pattern(const bdd::Node *node) const {
    if (node->get_type() != bdd::NodeType::CALL) {
      return false;
    }

    const bdd::Call *call_node = static_cast<const bdd::Call *>(node);
    const call_t &call = call_node->get_call();

    if (call.function_name != "nf_set_rte_ipv4_udptcp_checksum") {
      return false;
    }

    return true;
  }

  virtual std::optional<speculation_t>
  speculate(const EP *ep, const bdd::Node *node,
            const Context &ctx) const override {
    if (bdd_node_match_pattern(node))
      return ctx;
    return std::nullopt;
  }

  virtual std::vector<__generator_product_t>
  process_node(const EP *ep, const bdd::Node *node) const override {
    std::vector<__generator_product_t> products;

    if (!bdd_node_match_pattern(node)) {
      return products;
    }

    const bdd::Call *call_node = static_cast<const bdd::Call *>(node);
    const call_t &call = call_node->get_call();

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

} // namespace x86
} // namespace synapse
