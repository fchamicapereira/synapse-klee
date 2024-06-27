#pragma once

#include "call-paths-to-bdd.h"

#include "target.h"
#include "module.h"
#include "../execution_plan/execution_plan.h"

namespace synapse {

class Context;

struct speculation_t {
  Context ctx;
  std::unordered_set<bdd::node_id_t> skip;

  speculation_t(const Context &_ctx) : ctx(_ctx) {}

  speculation_t(const Context &_ctx,
                const std::unordered_set<bdd::node_id_t> &_skip)
      : ctx(_ctx), skip(_skip) {}
};

class ModuleGenerator {
protected:
  ModuleType type;
  TargetType target;
  std::string name;

public:
  ModuleGenerator(ModuleType _type, TargetType _target,
                  const std::string &_name)
      : type(_type), target(_target), name(_name) {}

  virtual ~ModuleGenerator() {}

  std::vector<const EP *> generate(const EP *ep, const bdd::Node *node,
                                   bool reorder_bdd) const;

  virtual std::optional<speculation_t>
  speculate(const EP *ep, const bdd::Node *node,
            const constraints_t &current_speculative_constraints,
            const Context &current_speculative_ctx) const = 0;

  ModuleType get_type() const { return type; }
  TargetType get_target() const { return target; }
  const std::string &get_name() const { return name; }

protected:
  virtual std::vector<const EP *> process_node(const EP *ep,
                                               const bdd::Node *node) const = 0;

  bool can_place(const EP *ep, const bdd::Call *call_node,
                 const std::string &obj_arg, PlacementDecision decision) const;
  bool check_placement(const EP *ep, const bdd::Call *call_node,
                       const std::string &obj_arg,
                       PlacementDecision decision) const;
  void place(EP *ep, addr_t obj, PlacementDecision decision) const;
};

} // namespace synapse