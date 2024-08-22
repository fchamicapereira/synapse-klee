#pragma once

#include <optional>

#include "call-paths-to-bdd.h"

#include "target.h"
#include "module.h"
#include "../execution_plan/execution_plan.h"
#include "../util.h"

namespace synapse {

class Context;

struct speculation_t {
  Context ctx;
  std::optional<TargetType> next_target;
  bdd::nodes_t skip;

  speculation_t(const Context &_ctx) : ctx(_ctx) {}
};

struct generator_product_t {
  const EP *ep;
  const std::string description;
  const bool bdd_reordered;

  generator_product_t(const EP *_ep, const std::string &_description)
      : ep(_ep), description(_description), bdd_reordered(false) {}

  generator_product_t(const EP *_ep, const std::string &_description,
                      bool _bdd_reordered)
      : ep(_ep), description(_description), bdd_reordered(_bdd_reordered) {}
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

  std::vector<generator_product_t> generate(const EP *ep, const bdd::Node *node,
                                            bool reorder_bdd) const;

  virtual std::optional<speculation_t>
  speculate(const EP *ep, const bdd::Node *node, const Context &ctx) const = 0;

  ModuleType get_type() const { return type; }
  TargetType get_target() const { return target; }
  const std::string &get_name() const { return name; }

protected:
  struct __generator_product_t {
    EP *ep;
    const std::string description;

    __generator_product_t(EP *_ep) : ep(_ep), description("") {}

    __generator_product_t(EP *_ep, const std::string &_description)
        : ep(_ep), description(_description) {}

    __generator_product_t(const __generator_product_t &other)
        : ep(other.ep), description(other.description) {}

    __generator_product_t(__generator_product_t &&other)
        : ep(std::move(other.ep)), description(std::move(other.description)) {}
  };

  virtual std::vector<__generator_product_t>
  process_node(const EP *ep, const bdd::Node *node) const = 0;

  bool can_place(const EP *ep, const bdd::Call *call_node,
                 const std::string &obj_arg, PlacementDecision decision) const;
  bool check_placement(const EP *ep, const bdd::Call *call_node,
                       const std::string &obj_arg,
                       PlacementDecision decision) const;
  void place(EP *ep, addr_t obj, PlacementDecision decision) const;
};

} // namespace synapse