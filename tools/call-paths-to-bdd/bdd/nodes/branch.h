#pragma once

#include "node.h"

namespace bdd {

class NodeManager;

class Branch : public Node {
private:
  Node *on_false;
  klee::ref<klee::Expr> condition;

public:
  Branch(node_id_t _id, const klee::ConstraintManager &_constraints,
         klee::ref<klee::Expr> _condition)
      : Node(_id, NodeType::BRANCH, _constraints), on_false(nullptr),
        condition(_condition) {}

  Branch(node_id_t _id, Node *_prev,
         const klee::ConstraintManager &_constraints, Node *_on_true,
         Node *_on_false, klee::ref<klee::Expr> _condition)
      : Node(_id, NodeType::BRANCH, _on_true, _prev, _constraints),
        on_false(_on_false), condition(_condition) {}

  klee::ref<klee::Expr> get_condition() const { return condition; }

  void set_condition(const klee::ref<klee::Expr> &_condition) {
    condition = _condition;
  }

  const Node *get_on_true() const { return next; }
  const Node *get_on_false() const { return on_false; }

  void set_on_true(Node *_on_true) { next = _on_true; }
  void set_on_false(Node *_on_false) { on_false = _on_false; }

  Node *get_mutable_on_true() { return next; }
  Node *get_mutable_on_false() { return on_false; }

  virtual std::vector<node_id_t> get_leaves() const override;
  virtual Node *clone(NodeManager &manager,
                      bool recursive = false) const override;

  void visit(BDDVisitor &visitor) const override;
  std::string dump(bool one_liner = false) const override;
};

} // namespace bdd