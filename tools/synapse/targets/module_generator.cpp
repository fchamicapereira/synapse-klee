#include "module_generator.h"

#include "../execution_plan/execution_plan.h"
#include "../log.h"

namespace synapse {

static bool can_process_platform(const EP *ep, TargetType target) {
  TargetType current_target = ep->get_current_platform();
  return current_target == target;
}

static void build_node_translations(translator_t &next_nodes_translator,
                                    translator_t &processed_nodes_translator,
                                    const bdd::BDD *old_bdd,
                                    const bdd::reorder_op_t &op) {
  next_nodes_translator[op.evicted_id] = op.candidate_info.id;

  for (bdd::node_id_t sibling_id : op.candidate_info.siblings) {
    if (sibling_id == op.candidate_info.id) {
      continue;
    }

    processed_nodes_translator[sibling_id] = op.candidate_info.id;

    const bdd::Node *sibling = old_bdd->get_node_by_id(sibling_id);
    assert(sibling->get_type() != bdd::NodeType::BRANCH);

    const bdd::Node *replacement = sibling->get_next();

    if (!replacement) {
      continue;
    }

    next_nodes_translator[sibling_id] = replacement->get_id();
  }
}

static bdd::anchor_info_t get_anchor_info(const EP *ep) {
  const bdd::Node *next = ep->get_next_node();
  assert(next);

  const bdd::Node *anchor = next->get_prev();
  assert(anchor);

  if (anchor->get_type() != bdd::NodeType::BRANCH) {
    return {anchor->get_id(), true};
  }

  const bdd::Branch *branch = static_cast<const bdd::Branch *>(anchor);

  if (branch->get_on_true() == next) {
    return {anchor->get_id(), true};
  }

  assert(branch->get_on_false() == next);
  return {anchor->get_id(), false};
}

static std::vector<EP *> get_reordered(const EP *ep) {
  std::vector<EP *> reordered;

  const bdd::Node *next = ep->get_next_node();

  if (!next) {
    return reordered;
  }

  const bdd::Node *node = next->get_prev();

  if (!node) {
    return reordered;
  }

  bdd::anchor_info_t anchor_info = get_anchor_info(ep);

  const bdd::BDD *bdd = ep->get_bdd();
  bool allow_shape_altering_ops = false;

  std::vector<bdd::reordered_bdd_t> new_bdds =
      bdd::reorder(bdd, anchor_info, allow_shape_altering_ops);

  for (const bdd::reordered_bdd_t &new_bdd : new_bdds) {
    bool is_ancestor = false;
    EP *new_ep = new EP(*ep, is_ancestor);

    translator_t next_nodes_translator;
    translator_t processed_nodes_translator;

    build_node_translations(next_nodes_translator, processed_nodes_translator,
                            bdd, new_bdd.op);
    assert(!new_bdd.op2.has_value());

    new_ep->replace_bdd(new_bdd.bdd, next_nodes_translator,
                        processed_nodes_translator);
    // new_ep->inspect();

    reordered.push_back(new_ep);
  }

  return reordered;
}

std::vector<generator_product_t>
ModuleGenerator::generate(const EP *ep, const bdd::Node *node,
                          bool reorder_bdd) const {
  if (!can_process_platform(ep, target)) {
    return {};
  }

  std::vector<__generator_product_t> node_products = process_node(ep, node);
  std::vector<generator_product_t> products;

  for (const __generator_product_t &node_product : node_products) {
    EP *ep = node_product.ep;
    Context &ctx = ep->get_mutable_ctx();
    ctx.update_throughput_estimates(ep);
    products.emplace_back(ep, node_product.description);
  }

  if (!reorder_bdd) {
    return products;
  }

  for (const __generator_product_t &product : node_products) {
    std::vector<EP *> reordered = get_reordered(product.ep);

    for (EP *reordered_ep : reordered) {
      Context &ctx = reordered_ep->get_mutable_ctx();
      ctx.update_throughput_estimates(reordered_ep);
      products.emplace_back(reordered_ep, product.description, true);
    }
  }

  return products;
}

bool ModuleGenerator::can_place(const EP *ep, const bdd::Call *call_node,
                                const std::string &obj_arg,
                                PlacementDecision decision) const {
  const call_t &call = call_node->get_call();

  assert(call.args.find(obj_arg) != call.args.end());
  klee::ref<klee::Expr> obj_expr = call.args.at(obj_arg).expr;
  addr_t obj = kutil::expr_addr_to_obj_addr(obj_expr);

  const Context &ctx = ep->get_ctx();
  return ctx.can_place(obj, decision);
}

bool ModuleGenerator::check_placement(const EP *ep, const bdd::Call *call_node,
                                      const std::string &obj_arg,
                                      PlacementDecision decision) const {
  const call_t &call = call_node->get_call();

  assert(call.args.find(obj_arg) != call.args.end());
  klee::ref<klee::Expr> obj_expr = call.args.at(obj_arg).expr;
  addr_t obj = kutil::expr_addr_to_obj_addr(obj_expr);

  const Context &ctx = ep->get_ctx();
  return ctx.check_placement(obj, decision);
}

void ModuleGenerator::place(EP *ep, addr_t obj,
                            PlacementDecision decision) const {
  Context &new_ctx = ep->get_mutable_ctx();
  new_ctx.save_placement(obj, decision);
}

} // namespace synapse