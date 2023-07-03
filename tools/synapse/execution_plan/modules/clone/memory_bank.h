#pragma once

#include <string>
#include <unordered_map>
#include <unordered_set>
#include <map>
#include <set>
#include <deque>

#include "bdd/nodes/node.h"
#include "call-paths-to-bdd.h"


namespace synapse {
namespace targets {
namespace clone {

class CloneMemoryBank : public TargetMemoryBank {

struct Origin {
  Target_ptr target;
  BDD::Node_ptr root;
  std::vector<BDD::Node_ptr> nodes;

  Origin(Target_ptr _target, BDD::Node_ptr _root) : target(_target), root(_root) {}
};

typedef std::shared_ptr<Origin> Origin_ptr;

private:
  std::deque<Origin_ptr> origins;
  std::map<BDD::node_id_t, Origin_ptr> root_to_origin;
  std::map<target_id_t, Origin_ptr> target_to_origin;

public:
  CloneMemoryBank() {}

  CloneMemoryBank(const CloneMemoryBank &mb) : 
    origins(mb.origins),
    root_to_origin(mb.root_to_origin),
    target_to_origin(mb.target_to_origin)
    {}

  virtual TargetMemoryBank_ptr clone() const override {
    auto clone = new CloneMemoryBank(*this);
    return TargetMemoryBank_ptr(clone);
  }

  Origin_ptr get_next_origin() const {
    if(origins.size() == 0) {
      return nullptr;
    }
    return origins[0];
  }

  void pop_origin() {
    assert(origins.size() > 0);
    origins.erase(origins.begin());
  }

  const std::map<target_id_t, Origin_ptr>& get_origins() const {
    return target_to_origin;
  }

  Origin_ptr get_origin_from_node(BDD::Node_ptr node) const {
    assert(root_to_origin.count(node->get_id()));
    return root_to_origin.at(node->get_id());
  }

  Origin_ptr get_origin_from_target(Target_ptr target) const {
    if(!target_to_origin.count(target->id)) {
      return nullptr;
    }
    return target_to_origin.at(target->id);
  }

  void add_origin(Target_ptr target, BDD::Node_ptr node) {
    auto origin = Origin_ptr(new Origin(target, node));
    origins.push_back(origin);
    root_to_origin[node->get_id()] = origin;  
    target_to_origin[target->id] = origin;
  }

  void add_origin_nodes(Target_ptr target, BDD::Node_ptr node) {
    assert(target_to_origin.count(target->id));
    auto origin = target_to_origin.at(target->id);
    origin->nodes.push_back(node);
  }
};

} // namespace clone
} // namespace targets
} // namespace synapse