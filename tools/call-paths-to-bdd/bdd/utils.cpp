#include "utils.h"

namespace BDD {

std::vector<Node_ptr> get_call_nodes(Node_ptr root,
                                     const std::vector<std::string> &fnames) {
  auto targets = std::vector<Node_ptr>();
  auto nodes = std::vector<Node_ptr>{root};

  while (nodes.size()) {
    auto node = *nodes.begin();
    nodes.erase(nodes.begin());

    if (node->get_type() == BDD::Node::NodeType::BRANCH) {
      auto branch_node = cast_node<Branch>(node);
      nodes.push_back(branch_node->get_on_true());
      nodes.push_back(branch_node->get_on_false());
      continue;
    }

    if (node->get_type() != BDD::Node::NodeType::CALL) {
      continue;
    }

    auto call_node = cast_node<Call>(node);
    auto call = call_node->get_call();

    nodes.push_back(call_node->get_next());

    auto found_it = std::find(fnames.begin(), fnames.end(), call.function_name);

    if (found_it == fnames.end()) {
      continue;
    }

    targets.push_back(node);
  }

  return targets;
}

unsigned extract_port(const Branch* branch) {
    auto kind = branch->get_condition()->getKind();
    assert(kind == klee::Expr::Kind::Eq);

    auto condition = branch->get_condition();
    auto right = condition->getKid(1);
    assert(right->getKind() == klee::Expr::Kind::Constant);
    auto casted = static_cast<klee::ConstantExpr*>(right.get());

    return casted->getZExtValue();
}

} // namespace BDD