#pragma once

#include <string>
#include <unordered_map>
#include <unordered_set>
#include <map>
#include <set>
#include <deque>

#include "bdd/nodes/node.h"
#include "call-paths-to-bdd.h"
#include "../../../symbex.h"


namespace synapse {
namespace targets {
namespace clone {

class CloneMemoryBank : public TargetMemoryBank {

struct StartingPoint {
  BDD::Node_ptr node;
  std::string arch;
  std::string device;
};

typedef std::shared_ptr<StartingPoint> StartingPoint_ptr;

private:
  std::deque<StartingPoint_ptr> starting_points;
  std::map<BDD::Node_ptr, std::string> node_to_target;
  bool is_inited = false;

public:
  CloneMemoryBank() {}

  CloneMemoryBank(const CloneMemoryBank &mb)
      :starting_points(mb.starting_points), node_to_target(mb.node_to_target), is_inited(mb.is_inited) {}

  virtual TargetMemoryBank_ptr clone() const override {
    auto clone = new CloneMemoryBank(*this);
    return TargetMemoryBank_ptr(clone);
  }

  StartingPoint_ptr get_next_starting_point() const {
    if(starting_points.size() == 0) {
      return nullptr;
    }
    return starting_points[0];
  }

  void pop_starting_point() {
    assert(starting_points.size() > 0);
    starting_points.erase(starting_points.begin());
  }

  std::string get_target_from_node(BDD::Node_ptr node) const {
    assert(node_to_target.find(node) != node_to_target.end());
    return node_to_target.at(node);
  }

  void add_starting_point(BDD::Node_ptr node, std::string target, std::string device) {
    starting_points.push_back(std::make_shared<StartingPoint>(StartingPoint{node, target, device}));
    node_to_target[node] = target;  
    is_inited = true;
  }

  bool is_initialised() const {
    return is_inited;
  }
};

} // namespace clone
} // namespace targets
} // namespace synapse