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

static std::vector<const EP *> get_reordered(const EP *ep) {
  std::vector<const EP *> reordered;

  const bdd::Node *next = ep->get_next_node();

  if (!next) {
    return reordered;
  }

  const bdd::Node *node = next->get_prev();

  if (!node) {
    return reordered;
  }

  const bdd::BDD *bdd = ep->get_bdd();
  bdd::node_id_t anchor_id = node->get_id();
  bool allow_shape_altering_ops = false;

  std::vector<bdd::reordered_bdd_t> new_bdds =
      bdd::reorder(bdd, anchor_id, allow_shape_altering_ops);

  for (const bdd::reordered_bdd_t &new_bdd : new_bdds) {
    EP *new_ep = new EP(*ep);

    translator_t next_nodes_translator;
    translator_t processed_nodes_translator;

    // std::cerr << "new ep: " << new_ep->get_id() << "\n";
    // std::cerr << "new_bdd: " << new_bdd.bdd->hash() << "\n";

    // auto DEBUG_CONDITION = new_bdd.op.candidate_info.siblings.size() > 1 &&
    //                        ep->get_leaves().size() > 1;
    // auto DEBUG_CONDITION = new_bdd.op.candidate_info.is_branch &&
    //                        new_bdd.op.candidate_info.siblings.size() > 1;
    // auto DEBUG_CONDITION = (new_ep->get_id() == 26601);

    // if (DEBUG_CONDITION) {
    //   std::cerr << "\n\n";
    //   std::cerr << "Anchor: " << anchor_id << "\n";
    //   std::cerr << "Evicted: " << new_bdd.op.evicted_id << "\n";
    //   std::cerr << "Candidate: " << new_bdd.op.candidate_info.id << "\n";
    //   std::cerr << "Siblings: ";
    //   for (bdd::node_id_t sibling_id : new_bdd.op.candidate_info.siblings) {
    //     std::cerr << sibling_id << " ";
    //   }
    //   std::cerr << "\n";
    //   if (new_bdd.op2.has_value()) {
    //     std::cerr << "Evicted2: " << new_bdd.op2->evicted_id << "\n";
    //     std::cerr << "Candidate2: " << new_bdd.op2->candidate_info.id <<
    //     "\n"; std::cerr << "Siblings2: "; for (bdd::node_id_t sibling_id :
    //     new_bdd.op2->candidate_info.siblings) {
    //       std::cerr << sibling_id << " ";
    //     }
    //     std::cerr << "\n";
    //   }
    //   std::cerr << "Old leaves:\n";
    //   for (auto leaf : ep->get_leaves()) {
    //     std::cerr << "  " << leaf.next->get_id() << "\n";
    //   }
    //   bdd::BDDVisualizer::visualize(bdd, false, {.fname = "old"});
    //   bdd::BDDVisualizer::visualize(new_bdd.bdd, false, {.fname = "new"});
    //   EPVisualizer::visualize(new_ep, false);
    // }

    build_node_translations(next_nodes_translator, processed_nodes_translator,
                            bdd, new_bdd.op);
    if (new_bdd.op2.has_value()) {
      build_node_translations(next_nodes_translator, processed_nodes_translator,
                              bdd, *new_bdd.op2);
    }

    // if (DEBUG_CONDITION) {
    //   std::cerr << "next_nodes_translator:\n";
    //   for (const auto &kv : next_nodes_translator) {
    //     std::cerr << "  " << kv.first << " -> " << kv.second << "\n";
    //   }

    //   std::cerr << "processed_nodes_translator:\n";
    //   for (const auto &kv : processed_nodes_translator) {
    //     std::cerr << "  " << kv.first << " -> " << kv.second << "\n";
    //   }
    // }

    new_ep->replace_bdd(new_bdd.bdd, next_nodes_translator,
                        processed_nodes_translator);
    new_ep->inspect();

    // if (DEBUG_CONDITION) {
    //   std::cerr << "New leaves:\n";
    //   for (auto leaf : new_ep->get_leaves()) {
    //     std::cerr << "  " << leaf.next->get_id() << "\n";
    //   }
    //   DEBUG_PAUSE
    // }
  }

  return reordered;
}

std::vector<const EP *> ModuleGenerator::generate(const EP *ep,
                                                  const bdd::Node *node,
                                                  bool reorder_bdd) const {
  std::vector<const EP *> new_eps;

  if (!can_process_platform(ep, target)) {
    return new_eps;
  }

  new_eps = process_node(ep, node);

  if (reorder_bdd) {
    std::vector<const EP *> reordered;

    for (const EP *ep : new_eps) {
      std::vector<const EP *> ep_reodered = get_reordered(ep);
      reordered.insert(reordered.end(), ep_reodered.begin(), ep_reodered.end());
    }

    if (reordered.size() > 0) {
      Log::dbg() << "+ " << reordered.size() << " reordered BDDs\n";
    }

    new_eps.insert(new_eps.end(), reordered.begin(), reordered.end());
  }

  return new_eps;
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