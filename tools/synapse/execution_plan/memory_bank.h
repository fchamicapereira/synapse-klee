#pragma once

#include "call-paths-to-bdd.h"
#include "klee-util.h"

#include <unordered_map>

#include "../generic.h"

namespace synapse {

class MemoryBank;
class TargetMemoryBank;

typedef std::shared_ptr<MemoryBank> MemoryBank_ptr;
typedef std::shared_ptr<TargetMemoryBank> TargetMemoryBank_ptr;

struct reorder_data_t {
  bool valid;
  int candidate_node_id;
  klee::ref<klee::Expr> condition;

  reorder_data_t() : valid(false) {}

  reorder_data_t(const reorder_data_t &other)
      : valid(other.valid), candidate_node_id(other.candidate_node_id),
        condition(other.condition) {}

  reorder_data_t(int _candidate_node_id, klee::ref<klee::Expr> _condition)
      : valid(true), candidate_node_id(_candidate_node_id),
        condition(_condition) {}
};

enum PlacementDecision {
  TofinoTable,
  TofinoTableSimple,
  TofinoRegister,
  IntegerAllocator,
  Counter,
};

struct expiration_data_t {
  bool valid;
  time_ns_t expiration_time;
  BDD::symbol_t number_of_freed_flows;

  expiration_data_t() : valid(false) {}

  expiration_data_t(time_ns_t _expiration_time,
                    const BDD::symbol_t &_number_of_freed_flows)
      : valid(true), expiration_time(_expiration_time),
        number_of_freed_flows(_number_of_freed_flows) {}
};

class MemoryBank {
protected:
  std::vector<reorder_data_t> reorder_data;
  std::unordered_map<addr_t, PlacementDecision> placement_decisions;
  std::unordered_set<BDD::node_id_t> can_be_ignored_bdd_nodes;
  expiration_data_t expiration_data;

public:
  MemoryBank() {}

  MemoryBank(const MemoryBank &mb)
      : reorder_data(mb.reorder_data),
        placement_decisions(mb.placement_decisions),
        can_be_ignored_bdd_nodes(mb.can_be_ignored_bdd_nodes),
        expiration_data(mb.expiration_data) {}

  void set_expiration_data(const expiration_data_t &_expiration_data) {
    expiration_data = _expiration_data;
  }

  const expiration_data_t &get_expiration_data() const {
    return expiration_data;
  }

  reorder_data_t get_reorder_data(int node_id) const {
    for (const auto &data : reorder_data) {
      if (data.valid && data.candidate_node_id == node_id) {
        return data;
      }
    }

    return reorder_data_t();
  }

  void add_reorder_data(int node_candidate_id, klee::ref<klee::Expr> cond) {
    reorder_data.emplace_back(node_candidate_id, cond);
  }

  void save_placement_decision(addr_t obj_addr, PlacementDecision decision) {
    placement_decisions[obj_addr] = decision;
  }

  bool has_placement_decision(addr_t obj_addr) {
    auto found_it = placement_decisions.find(obj_addr);
    return found_it != placement_decisions.end();
  }

  bool check_compatible_placement_decision(addr_t obj_addr,
                                           PlacementDecision decision) const {
    auto found_it = placement_decisions.find(obj_addr);

    return found_it == placement_decisions.end() ||
           found_it->second == decision;
  }

  bool check_placement_decision(addr_t obj_addr,
                                PlacementDecision decision) const {
    auto found_it = placement_decisions.find(obj_addr);

    return found_it != placement_decisions.end() &&
           found_it->second == decision;
  }

  bool check_if_can_be_ignored(BDD::Node_ptr node) const {
    auto id = node->get_id();
    return can_be_ignored_bdd_nodes.find(id) != can_be_ignored_bdd_nodes.end();
  }

  void can_be_ignored(BDD::Node_ptr node) {
    auto id = node->get_id();
    can_be_ignored_bdd_nodes.insert(id);
  }

  virtual MemoryBank_ptr clone() const {
    return MemoryBank_ptr(new MemoryBank(*this));
  }

  static MemoryBank_ptr build() { return MemoryBank_ptr(new MemoryBank()); }
};

class TargetMemoryBank {
public:
  virtual TargetMemoryBank_ptr clone() const = 0;
};

} // namespace synapse
