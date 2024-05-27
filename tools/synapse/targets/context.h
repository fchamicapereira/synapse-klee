#pragma once

#include "call-paths-to-bdd.h"
#include "klee-util.h"
#include "bdd-reorderer.h"

#include <unordered_map>

#include "../generic.h"

namespace synapse {

class Target;
enum class TargetType;

enum class PlacementDecision {
  TofinoTable,
  TofinoTableSimple,
  TofinoRegister,
  IntegerAllocator,
  Counter,
};

struct expiration_data_t {
  bool valid;
  time_ns_t expiration_time;
  symbol_t number_of_freed_flows;

  expiration_data_t() : valid(false) {}

  expiration_data_t(time_ns_t _expiration_time,
                    const symbol_t &_number_of_freed_flows)
      : valid(true), expiration_time(_expiration_time),
        number_of_freed_flows(_number_of_freed_flows) {}
};

class TargetContext {
public:
  TargetContext() {}
  virtual ~TargetContext() {}
  virtual TargetContext *clone() const = 0;
};

class Context {
private:
  std::vector<bdd::reorder_op_t> reorder_ops;
  std::unordered_map<addr_t, PlacementDecision> placement_decisions;
  bdd::nodes_t can_be_ignored_bdd_nodes;
  expiration_data_t expiration_data;
  std::unordered_map<TargetType, TargetContext *> target_ctxs;

public:
  Context(const std::vector<const Target *> &targets);
  Context(const Context &other);
  Context(Context &&other);

  ~Context();

  Context &operator=(const Context &other);

  void set_expiration_data(const expiration_data_t &_expiration_data);
  const expiration_data_t &get_expiration_data() const;

  template <class Ctx> Ctx *get_target_context(TargetType type) const;

  void add_reorder_op(const bdd::reorder_op_t &op);
  void save_placement_decision(addr_t obj_addr, PlacementDecision decision);
  bool has_placement_decision(addr_t obj_addr);
  void can_be_ignored(const bdd::Node *node);

  bool check_compatible_placement_decision(addr_t obj_addr,
                                           PlacementDecision decision) const;
  bool check_placement_decision(addr_t obj_addr,
                                PlacementDecision decision) const;
  bool check_if_can_be_ignored(const bdd::Node *node) const;
};

} // namespace synapse
