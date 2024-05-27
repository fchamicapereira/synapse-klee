#include "search_space.h"

namespace synapse {

static ss_node_id_t node_id_counter = 0;

void SearchSpace::activate_leaf(const EP &ep) {
  if (!root) {
    ss_node_id_t id = node_id_counter++;
    ep_id_t ep_id = ep.get_id();
    Score score = hcfg->get_score(ep);
    TargetType target = ep.get_current_platform();
    root = new SSNode(id, ep_id, score, target);
    leaves.push_back(root);
    active_leaf = root;
    return;
  }

  ep_id_t ep_id = ep.get_id();
  auto ss_node_matcher = [ep_id](SSNode *node) { return node->ep_id == ep_id; };
  auto found_it = std::find_if(leaves.begin(), leaves.end(), ss_node_matcher);
  assert(found_it != leaves.end() && "Leaf not found");

  active_leaf = *found_it;
  explored.insert(active_leaf->ep_id);
}

void SearchSpace::add_to_active_leaf(const bdd::Node *node,
                                     const Module *module,
                                     const std::vector<EP> &generated_eps) {
  assert(active_leaf && "Active leaf not set");

  for (const EP &ep : generated_eps) {
    ss_node_id_t id = node_id_counter++;
    ep_id_t ep_id = ep.get_id();
    Score score = hcfg->get_score(ep);
    TargetType target = ep.get_current_platform();
    SSNode *new_node = new SSNode(id, ep_id, score, target, module, node);
    active_leaf->children.push_back(new_node);
  }
}

SSNode *SearchSpace::get_root() const { return root; }

bool SearchSpace::was_explored(ss_node_id_t node_id) const {
  return explored.find(node_id) != explored.end();
}

} // namespace synapse