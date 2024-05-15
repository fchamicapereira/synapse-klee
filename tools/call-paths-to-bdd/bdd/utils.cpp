#include "utils.h"

namespace bdd {

std::vector<const Call *>
get_call_nodes(const Node *root, const std::vector<std::string> &fnames) {
  std::vector<const Call *> targets;
  std::vector<const Node *> nodes{root};

  while (nodes.size()) {
    const Node *node = *nodes.begin();
    nodes.erase(nodes.begin());

    if (node->get_type() == NodeType::BRANCH) {
      const Branch *branch_node = static_cast<const Branch *>(node);
      nodes.push_back(branch_node->get_on_true());
      nodes.push_back(branch_node->get_on_false());
      continue;
    }

    if (node->get_type() != NodeType::CALL) {
      continue;
    }

    const Call *call_node = static_cast<const Call *>(node);
    const call_t &call = call_node->get_call();

    nodes.push_back(call_node->get_next());

    auto found_it = std::find(fnames.begin(), fnames.end(), call.function_name);
    if (found_it == fnames.end())
      continue;

    targets.push_back(call_node);
  }

  return targets;
}

} // namespace bdd