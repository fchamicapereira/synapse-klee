#pragma once

#include "node.h"

namespace bdd {

enum class RouteOperation { FWD, DROP, BCAST };

class Route : public Node {
private:
  RouteOperation operation;
  int dst_device;

public:
  Route(node_id_t _id, const klee::ConstraintManager &_constraints,
        RouteOperation _operation, int _dst_device)
      : Node(_id, NodeType::ROUTE, _constraints), operation(_operation),
        dst_device(_dst_device) {}

  Route(node_id_t _id, const klee::ConstraintManager &_constraints,
        RouteOperation _operation)
      : Node(_id, NodeType::ROUTE, _constraints), operation(_operation) {
    assert(operation == RouteOperation::DROP ||
           operation == RouteOperation::BCAST);
  }

  Route(node_id_t _id, Node *_next, Node *_prev,
        const klee::ConstraintManager &_constraints, RouteOperation _operation,
        int _dst_device)
      : Node(_id, NodeType::ROUTE, _next, _prev, _constraints),
        operation(_operation), dst_device(_dst_device) {}

  int get_dst_device() const { return dst_device; }
  RouteOperation get_operation() const { return operation; }

  virtual Node *clone(NodeManager &manager,
                      bool recursive = false) const override;

  void visit(BDDVisitor &visitor) const override;
  std::string dump(bool one_liner = false) const;
};

} // namespace bdd