#pragma once

#include "call-paths-to-bdd.h"

#include "target.h"
#include "module.h"
#include "../execution_plan/execution_plan.h"

namespace synapse {

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