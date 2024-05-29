#include "module_generator.h"

#include "../execution_plan/execution_plan.h"
#include "../log.h"

namespace synapse {

static bool can_process_platform(const EP *ep, TargetType target) {
  auto current_target = ep->get_current_platform();
  return current_target == target;
}

static std::vector<const EP *> get_reordered(const EP *ep) {
  std::vector<const EP *> reordered;

  assert(false && "TODO");
  // auto next_node = ep->get_next_node();

  // if (!next_node) {
  //   return reordered;
  // }

  // auto current_node = next_node->get_prev();

  // if (!current_node) {
  //   return reordered;
  // }

  // auto current_bdd = ep->get_bdd();
  // auto current_target = ep->get_current_platform();

  // const auto &meta = ep->get_meta();
  // auto roots_per_target = meta.roots_per_target.at(current_target);

  // auto reordered_bdds =
  //     bdd::reorder(current_bdd, current_node, roots_per_target);

  // for (auto reordered_bdd : reordered_bdds) {
  //   auto ep_cloned = ep->clone(reordered_bdd.bdd);

  //   if (!reordered_bdd.condition.isNull()) {
  //     auto ctx = ep_cloned.get_context();
  //     ctx->add_reorder_op(reordered_bdd.candidate->get_id(),
  //                          reordered_bdd.condition);
  //   }

  //   ep_cloned.sync_leaves_nodes(reordered_bdd.candidate, false);
  //   ep_cloned.inc_reordered_nodes();

  //   // If the next node was a BDD starting point, then actually the starting
  //   // point becomes the candidate node.
  //   ep_cloned.replace_roots(next_node->get_id(),
  //                           reordered_bdd.candidate->get_id());

  //   reordered.push_back(ep_cloned);
  // }

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

} // namespace synapse