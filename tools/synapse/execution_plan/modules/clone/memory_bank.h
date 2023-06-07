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
private:
  std::map<std::string, BDD::Node_ptr> m_starting_points;
  std::map<BDD::Node_ptr, std::string> m_starting_points_reverse;
  std::set<BDD::node_id_t> s_node_ids;
  std::deque<BDD::Node_ptr> q_start_nodes;
  bool is_inited = false;

public:
  CloneMemoryBank() {}

  CloneMemoryBank(const CloneMemoryBank &mb)
      :m_starting_points(mb.m_starting_points), m_starting_points_reverse(mb.m_starting_points_reverse),
      s_node_ids(mb.s_node_ids), q_start_nodes(mb.q_start_nodes), is_inited(mb.is_inited) {}

  virtual TargetMemoryBank_ptr clone() const override {
    auto clone = new CloneMemoryBank(*this);
    return TargetMemoryBank_ptr(clone);
  }

  const std::string& get_next_device() const {
    return m_starting_points.begin()->first;
  }

  BDD::Node_ptr get_next_metanode() {
    if(m_starting_points.size() == 0) {
      return nullptr;
    }
    return m_starting_points.begin()->second;
  }

  std::string get_target_from_node(BDD::Node_ptr node) {
    if(m_starting_points_reverse.find(node) == m_starting_points_reverse.end()) {
      return nullptr;
    }
    return m_starting_points_reverse[node];
  }

  void pop_metanode() {
    assert(m_starting_points.size() > 0);
    m_starting_points.erase(m_starting_points.begin());
  }

  BDD::Node_ptr get_next_node() const {
    if(q_start_nodes.size() == 0) {
      return nullptr;
    }
    return q_start_nodes.front();
  }

  void pop_next_node() {
    assert(q_start_nodes.size() > 0);
    q_start_nodes.pop_front();
  }

  void add_starting_point(std::string device, BDD::Node_ptr node, std::string arch) {
    if(m_starting_points.find(device) == m_starting_points.end()) {
      m_starting_points.emplace(device, node);
      m_starting_points_reverse.emplace(node, arch);
      s_node_ids.insert(node->get_id());
      q_start_nodes.push_back(node);
      is_inited = true;
    }
  }

  bool is_starting_point(BDD::Node_ptr node) const {
    return s_node_ids.find(node->get_id()) != s_node_ids.end();
  }

  const BDD::Node_ptr& get_starting_point(std::string device) {
    return m_starting_points[device];
  }

  const std::map<std::string, BDD::Node_ptr>& get_starting_points() const {
    return m_starting_points;
  }

  bool is_initialised() const {
    return is_inited;
  }
};

} // namespace clone
} // namespace targets
} // namespace synapse