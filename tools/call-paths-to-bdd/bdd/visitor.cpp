#include "visitor.h"
#include "bdd.h"
#include "nodes/nodes.h"

namespace bdd {
void BDDVisitor::visit(const Branch *node) {
  if (!node)
    return;

  const Node *on_true = node->get_on_true();
  const Node *on_false = node->get_on_false();

  BDDVisitorAction action = visitBranch(node);
  if (action == BDDVisitorAction::VISIT_CHILDREN) {
    on_true->visit(*this);
    on_false->visit(*this);
  }
}

void BDDVisitor::visit(const Call *node) {
  if (!node)
    return;

  const Node *next = node->get_next();

  BDDVisitorAction action = visitCall(node);
  if (action == BDDVisitorAction::VISIT_CHILDREN && next)
    next->visit(*this);
}

void BDDVisitor::visit(const Route *node) {
  if (!node)
    return;

  const Node *next = node->get_next();

  BDDVisitorAction action = visitRoute(node);
  if (action == BDDVisitorAction::VISIT_CHILDREN && next)
    next->visit(*this);
}

void BDDVisitor::visitRoot(const Node *root) {
  if (!root)
    return;
  root->visit(*this);
}

void BDDVisitor::visit(const BDD *bdd) {
  const Node *root = bdd->get_root();
  assert(root);
  visitRoot(root);
}

} // namespace bdd