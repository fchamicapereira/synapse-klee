#include "branch.h"
#include "manager.h"
#include "../visitor.h"

#include "klee-util.h"

namespace bdd {

Node *Branch::clone(NodeManager &manager, bool recursive) const {
  Node *clone;

  const Node *on_true = get_on_true();
  const Node *on_false = get_on_false();

  if (recursive && on_true && on_false) {
    Node *on_true_clone = on_true->clone(manager, true);
    Node *on_false_clone = on_false->clone(manager, true);
    clone = new Branch(id, nullptr, constraints, on_true_clone, on_false_clone,
                       condition);
    on_true_clone->set_prev(clone);
    on_false_clone->set_prev(clone);
  } else {
    clone = new Branch(id, constraints, condition);
  }

  manager.add_node(clone);
  return clone;
}

std::vector<node_id_t> Branch::get_terminating_node_ids() const {
  std::vector<node_id_t> terminating_ids;

  assert(next);
  assert(on_false);

  auto on_true_ids = next->get_terminating_node_ids();
  auto on_false_ids = on_false->get_terminating_node_ids();

  terminating_ids.insert(terminating_ids.end(), on_true_ids.begin(),
                         on_true_ids.end());

  terminating_ids.insert(terminating_ids.end(), on_false_ids.begin(),
                         on_false_ids.end());

  return terminating_ids;
}

void Branch::visit(BDDVisitor &visitor) const { visitor.visit(this); }

std::string Branch::dump(bool one_liner) const {
  std::stringstream ss;
  ss << id << ":";
  ss << "if (";
  ss << kutil::expr_to_string(condition, one_liner);
  ss << ")";
  return ss.str();
}

std::string Branch::dump_recursive(int lvl) const {
  std::stringstream result;

  result << Node::dump_recursive(lvl);
  result << on_false->dump_recursive(lvl + 1);

  return result.str();
}

} // namespace bdd