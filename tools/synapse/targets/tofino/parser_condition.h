#pragma once

#include "tofino_module.h"

namespace synapse {
namespace tofino {

class ParserCondition : public TofinoModule {
private:
  klee::ref<klee::Expr> original_condition;

  klee::ref<klee::Expr> conditional_hdr;
  klee::ref<klee::Expr> hdr_field;
  std::vector<int> hdr_values;

public:
  ParserCondition(const bdd::Node *node,
                  klee::ref<klee::Expr> _original_condition,
                  klee::ref<klee::Expr> _conditional_hdr,
                  klee::ref<klee::Expr> _hdr_field,
                  const std::vector<int> &_hdr_values)
      : TofinoModule(ModuleType::Tofino_ParserCondition, "ParserCondition",
                     node),
        original_condition(_original_condition),
        conditional_hdr(_conditional_hdr), hdr_field(_hdr_field),
        hdr_values(_hdr_values) {}

  virtual void visit(EPVisitor &visitor, const EP *ep,
                     const EPNode *ep_node) const override {
    visitor.visit(ep, ep_node, this);
  }

  virtual Module *clone() const {
    ParserCondition *cloned = new ParserCondition(
        node, original_condition, conditional_hdr, hdr_field, hdr_values);
    return cloned;
  }

  klee::ref<klee::Expr> get_conditional_hdr() const { return conditional_hdr; }
  klee::ref<klee::Expr> get_hdr_field() const { return hdr_field; }
  const std::vector<int> &get_hdr_values() const { return hdr_values; }
};

class ParserConditionGenerator : public TofinoModuleGenerator {
public:
  ParserConditionGenerator()
      : TofinoModuleGenerator(ModuleType::Tofino_ParserCondition,
                              "ParserCondition") {}

protected:
  virtual std::vector<const EP *>
  process_node(const EP *ep, const bdd::Node *node) const override {
    std::vector<const EP *> new_eps;

    if (node->get_type() != bdd::NodeType::BRANCH) {
      return new_eps;
    }

    const bdd::Branch *branch_node = static_cast<const bdd::Branch *>(node);

    if (!is_parser_condition(branch_node)) {
      return new_eps;
    }

    klee::ref<klee::Expr> original_condition = branch_node->get_condition();

    klee::ref<klee::Expr> field;
    std::vector<int> values;
    build_parser_select(original_condition, field, values);

    const bdd::Node *on_true = branch_node->get_on_true();
    const bdd::Node *on_false = branch_node->get_on_false();

    assert(on_true);
    assert(on_false);

    std::vector<const bdd::Node *> on_true_borrows =
        get_future_functions(on_true, {"packet_borrow_next_chunk"}, true);
    std::vector<const bdd::Node *> on_false_borrows =
        get_future_functions(on_false, {"packet_borrow_next_chunk"}, true);

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

    assert(branch_node->get_on_true());
    assert(branch_node->get_on_false());

    EP *new_ep = new EP(*ep);
    new_eps.push_back(new_ep);

    Module *if_module = new ParserCondition(node, original_condition,
                                            conditional_hdr, field, values);
    Module *then_module = new Then(node);
    Module *else_module = new Else(node);

    EPNode *if_node = new EPNode(if_module);
    EPNode *then_node = new EPNode(then_module);
    EPNode *else_node = new EPNode(else_module);

    if_node->set_children({then_node, else_node});
    then_node->set_prev(if_node);
    else_node->set_prev(if_node);

    new_ep->update_node_constraints(then_node, else_node, original_condition);

    EPLeaf then_leaf(then_node, branch_node->get_on_true());
    EPLeaf else_leaf(else_node, branch_node->get_on_false());

    new_ep->process_leaf(if_node, {then_leaf, else_leaf});

    TofinoContext *tofino_ctx = get_mutable_tofino_ctx(new_ep);
    tofino_ctx->parser_select(ep, node, field, values);

    return new_eps;
  }

private:
  void parse_parser_select(klee::ref<klee::Expr> expr,
                           klee::ref<klee::Expr> &field, int &value) const {
    assert(expr->getKind() == klee::Expr::Kind::Eq);

    klee::ref<klee::Expr> lhs = expr->getKid(0);
    klee::ref<klee::Expr> rhs = expr->getKid(1);

    if (field.isNull()) {
      bool lhs_is_readLSB = kutil::is_packet_readLSB(lhs);
      bool rhs_is_readLSB = kutil::is_packet_readLSB(rhs);

      assert(lhs_is_readLSB || rhs_is_readLSB);
      assert(lhs_is_readLSB != rhs_is_readLSB);

      field = lhs_is_readLSB ? lhs : rhs;
    }

    bool lhs_is_field =
        kutil::solver_toolbox.are_exprs_always_equal(lhs, field);
    bool rhs_is_field =
        kutil::solver_toolbox.are_exprs_always_equal(rhs, field);
    assert(lhs_is_field || rhs_is_field);

    klee::ref<klee::Expr> value_expr = lhs_is_field ? rhs : lhs;
    assert(value_expr->getKind() == klee::Expr::Kind::Constant);

    value = kutil::solver_toolbox.value_from_expr(value_expr);
  }

  void build_parser_select(klee::ref<klee::Expr> condition,
                           klee::ref<klee::Expr> &field,
                           std::vector<int> &values) const {
    condition = kutil::filter(condition, {"packet_chunks"});
    condition = kutil::swap_packet_endianness(condition);
    condition = kutil::simplify(condition);

    switch (condition->getKind()) {
    case klee::Expr::Kind::Or: {
      klee::ref<klee::Expr> lhs = condition->getKid(0);
      klee::ref<klee::Expr> rhs = condition->getKid(1);
      build_parser_select(lhs, field, values);
      build_parser_select(rhs, field, values);
    } break;
    case klee::Expr::Kind::Eq: {
      int value;
      parse_parser_select(condition, field, value);
      values.push_back(value);
    } break;
    default: {
      assert(false && "Not implemented");
    }
    }
  }
};

} // namespace tofino
} // namespace synapse
