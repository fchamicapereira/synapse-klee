#pragma once

#include "node.h"

namespace bdd {

class Route : public Node {
public:
  enum Operation { FWD, DROP, BCAST };

private:
  Operation operation;
  int dst_device;

public:
  Route(node_id_t _id, const klee::ConstraintManager &_constraints,
        Operation _operation, int _dst_device)
      : Node(_id, Node::NodeType::ROUTE, _constraints), operation(_operation),
        dst_device(_dst_device) {
    assert(operation == Operation::FWD);
  }

  Route(node_id_t _id, const klee::ConstraintManager &_constraints,
        Operation _operation)
      : Node(_id, Node::NodeType::ROUTE, _constraints), operation(_operation) {
    assert(operation == Operation::DROP || operation == Operation::BCAST);
  }

  Route(node_id_t _id, Node *_next, Node *_prev,
        const klee::ConstraintManager &_constraints, Operation _operation,
        int _dst_device)
      : Node(_id, Node::NodeType::ROUTE, _next, _prev, _constraints),
        operation(_operation), dst_device(_dst_device) {}

  int get_dst_device() const { return dst_device; }
  Operation get_operation() const { return operation; }

  virtual Node *clone(NodeManager &manager,
                      bool recursive = false) const override;

  void visit(BDDVisitor &visitor) const override;
  std::string dump(bool one_liner = false) const;
};

} // namespace bdd