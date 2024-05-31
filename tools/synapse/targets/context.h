#pragma once

#include "call-paths-to-bdd.h"
#include "klee-util.h"
#include "bdd-reorderer.h"

#include <unordered_map>

#include "../util.h"

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
  std::unordered_map<addr_t, bdd::map_config_t> map_configs;
  std::unordered_map<addr_t, bdd::vector_config_t> vector_configs;
  std::unordered_map<addr_t, bdd::dchain_config_t> dchain_configs;
  std::unordered_map<addr_t, bdd::sketch_config_t> sketch_configs;
  std::unordered_map<addr_t, bdd::cht_config_t> cht_configs;

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

  bool has_map_config(addr_t addr) const;
  bool has_vector_config(addr_t addr) const;
  bool has_dchain_config(addr_t addr) const;
  bool has_sketch_config(addr_t addr) const;
  bool has_cht_config(addr_t addr) const;

  const bdd::map_config_t &get_map_config(addr_t addr) const;
  const bdd::vector_config_t &get_vector_config(addr_t addr) const;
  const bdd::dchain_config_t &get_dchain_config(addr_t addr) const;
  const bdd::sketch_config_t &get_sketch_config(addr_t addr) const;
  const bdd::cht_config_t &get_cht_config(addr_t addr) const;

  void save_map_config(addr_t addr, bdd::map_config_t cfg);
  void save_vector_config(addr_t addr, bdd::vector_config_t cfg);
  void save_dchain_config(addr_t addr, bdd::dchain_config_t cfg);
  void save_sketch_config(addr_t addr, bdd::sketch_config_t cfg);
  void save_cht_config(addr_t addr, bdd::cht_config_t cfg);

  void set_expiration_data(const expiration_data_t &_expiration_data);
  const expiration_data_t &get_expiration_data() const;

  const TargetContext *get_target_context(TargetType type) const;
  TargetContext *get_mutable_target_context(TargetType type);

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
