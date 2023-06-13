#pragma once

#include "../module.h"
#include "data_structures/data_structures.h"
#include "memory_bank.h"

using BDD::symbex::CHUNK;

namespace synapse {
namespace targets {
namespace tofino {

class TofinoModule : public Module {
public:
  TofinoModule(ModuleType _type, const char *_name)
      : Module(_type, TargetType::Tofino, _name) {}

  TofinoModule(ModuleType _type, const char *_name, BDD::Node_ptr node)
      : Module(_type, TargetType::Tofino, _name, node) {}

protected:
  // For parsing conditions, they can only look at the packet, so we must ignore
  // other symbols (e.g. packet length).
  klee::ref<klee::Expr>
  cleanup_parsing_condition(klee::ref<klee::Expr> condition) const {
    auto parser_cond = kutil::filter(condition, {BDD::symbex::CHUNK});

    parser_cond = kutil::swap_packet_endianness(parser_cond);
    parser_cond = kutil::simplify(parser_cond);

    return parser_cond;
  }

  // A parser condition should be the single discriminating condition that
  // decides whether a parsing state is performed or not. In BDD language, it
  // decides if a specific packet_borrow_next_chunk is applied.
  //
  // One classic example would be condition that checks if the ethertype field
  // on the ethernet header equals the IP protocol.
  //
  // A branch condition is considered a parsing condition if:
  //   - Has pending chunks to be borrowed in the future
  //   - Only looks at the packet
  bool is_parser_condition(BDD::Node_ptr node) const {
    auto casted = BDD::cast_node<BDD::Branch>(node);

    if (!casted) {
      return false;
    }

    auto future_borrows =
        get_all_functions_after_node(node, {BDD::symbex::FN_BORROW_CHUNK});

    if (future_borrows.size() == 0) {
      return false;
    }

    auto condition = casted->get_condition();
    auto only_looks_at_packet = is_expr_only_packet_dependent(condition);

    return only_looks_at_packet;
  }

  processing_result_t postpone(const ExecutionPlan &ep, BDD::Node_ptr node,
                               Module_ptr new_module) const;
  ExecutionPlan apply_postponed(ExecutionPlan ep, BDD::Node_ptr current_node,
                                BDD::Node_ptr next_node) const;
  processing_result_t ignore(const ExecutionPlan &ep, BDD::Node_ptr node) const;

public:
  virtual void visit(ExecutionPlanVisitor &visitor,
                     const ExecutionPlanNode *ep_node) const = 0;
  virtual Module_ptr clone() const = 0;
  virtual bool equals(const Module *other) const = 0;
};

} // namespace tofino
} // namespace targets
} // namespace synapse
