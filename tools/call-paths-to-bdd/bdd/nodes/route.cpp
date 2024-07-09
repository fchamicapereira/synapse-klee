#include "route.h"
#include "manager.h"
#include "../visitor.h"

namespace bdd {

Node *Route::clone(NodeManager &manager, bool recursive) const {
  Route *clone;

  if (recursive && next) {
    Node *next_clone = next->clone(manager, true);
    clone =
        new Route(id, next_clone, nullptr, constraints, operation, dst_device);
    next_clone->set_prev(clone);
  } else {
    clone = new Route(id, constraints, operation, dst_device);
  }

  manager.add_node(clone);
  return clone;
}

void Route::visit(BDDVisitor &visitor) const { visitor.visit(this); }

std::string Route::dump(bool one_liner, bool id_name_only) const {
  std::stringstream ss;
  ss << id << ":";

  switch (operation) {
  case RouteOperation::FWD:
    ss << "FORWARD";
    break;
  case RouteOperation::DROP:
    ss << "DROP";
    break;
  case RouteOperation::BCAST:
    ss << "BROADCAST";
    break;
  }

  return ss.str();
}

} // namespace bdd