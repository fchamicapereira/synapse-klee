#pragma once

#include <assert.h>

namespace bdd {

class BDD;

class Node;
class Branch;
class Call;
class Route;

enum class BDDVisitorAction { VISIT_CHILDREN, STOP };

class BDDVisitor {
public:
  virtual void visit(const BDD &bdd);

  void visit(const Branch *node);
  void visit(const Call *node);
  void visit(const Route *node);

protected:
  virtual BDDVisitorAction visitBranch(const Branch *node) = 0;
  virtual BDDVisitorAction visitCall(const Call *node) = 0;
  virtual BDDVisitorAction visitRoute(const Route *node) = 0;
  virtual void visitRoot(const Node *root);
};

} // namespace bdd
