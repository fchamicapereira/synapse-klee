#pragma once

#include "tofino_module.h"

#include "else.h"
#include "then.h"

namespace synapse {
namespace tofino {

class IfHeaderValid : public TofinoModule {
private:
  klee::ref<klee::Expr> hdr;
  klee::ref<klee::Expr> condition;

public:
  IfHeaderValid(const bdd::Node *node, klee::ref<klee::Expr> _hdr,
                klee::ref<klee::Expr> _condition)
      : TofinoModule(ModuleType::Tofino_IfHeaderValid, "IfHeaderValid", node),
        condition(_condition) {}

  virtual void visit(EPVisitor &visitor, const EPNode *ep_node) const override {
    visitor.visit(ep_node, this);
  }

  virtual Module *clone() const {
    IfHeaderValid *cloned = new IfHeaderValid(node, condition, hdr);
    return cloned;
  }

  klee::ref<klee::Expr> get_condition() const { return condition; }
  klee::ref<klee::Expr> get_hdr() const { return hdr; }
};

class IfHeaderValidGenerator : public TofinoModuleGenerator {
public:
  IfHeaderValidGenerator()
      : TofinoModuleGenerator(ModuleType::Tofino_IfHeaderValid,
                              "IfHeaderValid") {}

protected:
  virtual std::vector<const EP *>
  process_node(const EP *ep, const bdd::Node *node) const override {
    std::vector<const EP *> new_eps;

    if (node->get_type() != bdd::NodeType::BRANCH) {
      return new_eps;
    }

    const bdd::Branch *branch_node = static_cast<const bdd::Branch *>(node);

    if (!is_parser_condition(node)) {
      return new_eps;
    }

    klee::ref<klee::Expr> condition =
        build_parsing_condition(branch_node->get_condition());

    const bdd::Node *on_true = branch_node->get_on_true();
    const bdd::Node *on_false = branch_node->get_on_false();

    assert(on_true);
    assert(on_false);

    std::vector<const bdd::Node *> on_true_borrows =
        get_all_functions_after_node(on_true, {"packet_borrow_next_chunk"},
                                     true);
    std::vector<const bdd::Node *> on_false_borrows =
        get_all_functions_after_node(on_false, {"packet_borrow_next_chunk"},
                                     true);

    // We are working under the assumption that before parsing a header we
    // always perform some kind of checking. Thus, we will never encounter
    // borrows on both sides (directly at least).
    assert(on_true_borrows.size() > 0 || on_false_borrows.size() > 0);
    assert(on_true_borrows.size() != on_false_borrows.size());

    const bdd::Node *conditional_borrow;
    const bdd::Node *not_conditional_path;

    if (on_true_borrows.size() > on_false_borrows.size()) {
      conditional_borrow = on_true_borrows[0];
      not_conditional_path = on_false;
    } else {
      conditional_borrow = on_false_borrows[0];
      not_conditional_path = on_true;
    }

    // Missing implementation of discriminating parsing between multiple
    // headers.
    // Right now we are assuming that either we parse the target header, or we
    // drop the packet.
    assert(is_parser_drop(not_conditional_path) && "Not implemented");

    klee::ref<klee::Expr> conditional_hdr =
        get_chunk_from_borrow(conditional_borrow);

    // Relevant for IPv4 options, but left for future work.
    assert(!borrow_has_var_len(conditional_borrow) && "Not implemented");

    Module *if_module = new IfHeaderValid(node, condition, conditional_hdr);
    Module *then_module = new Then(node);
    Module *else_module = new Else(node);

    EPNode *if_node = new EPNode(if_module);
    EPNode *then_node = new EPNode(then_module);
    EPNode *else_node = new EPNode(else_module);

    if_node->set_children({then_node, else_node});
    then_node->set_prev(if_node);
    else_node->set_prev(if_node);

    EPLeaf then_leaf(then_node, branch_node->get_on_true());
    EPLeaf else_leaf(else_node, branch_node->get_on_false());

    EP *new_ep = new EP(*ep);
    new_eps.push_back(new_ep);

    TNA &tna = get_mutable_tna(new_ep);
    tna.update_parser_condition(ep, condition);

    new_ep->process_leaf(if_node, {then_leaf, else_leaf});
    new_ep->process_future_node(conditional_borrow);

    return new_eps;
  }

private:
  // For parsing conditions, they can only look at the packet, so we must ignore
  // other symbols (e.g. packet length).
  klee::ref<klee::Expr>
  build_parsing_condition(klee::ref<klee::Expr> condition) const {
    klee::ref<klee::Expr> parser_cond =
        kutil::filter(condition, {"packet_chunks"});

    parser_cond = kutil::swap_packet_endianness(parser_cond);
    parser_cond = kutil::simplify(parser_cond);

    return parser_cond;
  }
};

} // namespace tofino
} // namespace synapse
