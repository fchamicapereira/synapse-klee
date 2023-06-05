#pragma once

#include <string>
#include <unordered_map>
#include <unordered_set>
#include <map>

#include "bdd/nodes/node.h"
#include "call-paths-to-bdd.h"
#include "../../../symbex.h"


namespace synapse {
namespace targets {
namespace clone {

class CloneMemoryBank : public TargetMemoryBank {
private:
  std::map<std::string, BDD::Node_ptr> m_starting_points;
public:
  virtual TargetMemoryBank_ptr clone() const override {
    return TargetMemoryBank_ptr(new CloneMemoryBank());
  }

  const std::string& get_next_device() const {
    return m_starting_points.begin()->first;
  }

  const BDD::Node_ptr& get_next_node() {
    return m_starting_points.begin()->second;
  }

  void pop_next() {
    assert(m_starting_points.size() > 0);
    m_starting_points.erase(m_starting_points.begin());
  }

  void add_starting_point(std::string device, BDD::Node_ptr node) {
    if(m_starting_points.find(device) == m_starting_points.end()) {
      m_starting_points.emplace(device, node);
    }
  }

  const BDD::Node_ptr& get_starting_point(std::string device) {
    return m_starting_points[device];
  }

  const std::map<std::string, BDD::Node_ptr>& get_starting_points() const {
    return m_starting_points;
  }

  bool has_starting_points() const {
    return !m_starting_points.empty();
  }
};

} // namespace clone
} // namespace targets
} // namespace synapse