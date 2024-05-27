#include "context.h"
#include "target.h"

namespace synapse {

Context::Context(const std::vector<const Target *> &targets) {
  for (const Target *target : targets) {
    target_ctxs[target->type] = target->ctx->clone();
  }
}

Context::Context(const Context &other)
    : reorder_ops(other.reorder_ops),
      placement_decisions(other.placement_decisions),
      can_be_ignored_bdd_nodes(other.can_be_ignored_bdd_nodes),
      expiration_data(other.expiration_data) {
  for (auto &target_ctx_pair : other.target_ctxs) {
    target_ctxs[target_ctx_pair.first] = target_ctx_pair.second->clone();
  }
}

Context::Context(Context &&other)
    : reorder_ops(std::move(other.reorder_ops)),
      placement_decisions(std::move(other.placement_decisions)),
      can_be_ignored_bdd_nodes(std::move(other.can_be_ignored_bdd_nodes)),
      expiration_data(std::move(other.expiration_data)),
      target_ctxs(std::move(other.target_ctxs)) {}

Context::~Context() {
  for (auto &target_ctx_pair : target_ctxs) {
    if (target_ctx_pair.second) {
      delete target_ctx_pair.second;
      target_ctx_pair.second = nullptr;
    }
  }
}

Context &Context::operator=(const Context &other) {
  if (this == &other) {
    return *this;
  }

  for (auto &target_ctx_pair : target_ctxs) {
    if (target_ctx_pair.second) {
      delete target_ctx_pair.second;
      target_ctx_pair.second = nullptr;
    }
  }

  reorder_ops = other.reorder_ops;
  placement_decisions = other.placement_decisions;
  can_be_ignored_bdd_nodes = other.can_be_ignored_bdd_nodes;
  expiration_data = other.expiration_data;

  for (auto &target_ctx_pair : other.target_ctxs) {
    target_ctxs[target_ctx_pair.first] = target_ctx_pair.second->clone();
  }

  return *this;
}

void Context::set_expiration_data(const expiration_data_t &_expiration_data) {
  expiration_data = _expiration_data;
}

const expiration_data_t &Context::get_expiration_data() const {
  return expiration_data;
}

template <class Ctx> Ctx *Context::get_target_context(TargetType type) const {
  static_assert(std::is_base_of<TargetContext, Ctx>::value,
                "MB not derived from TargetContext");
  assert(target_ctxs.find(type) != target_ctxs.end());
  return static_cast<Ctx *>(target_ctxs.at(type));
}

void Context::add_reorder_op(const bdd::reorder_op_t &op) {
  reorder_ops.push_back(op);
}

void Context::save_placement_decision(addr_t obj_addr,
                                      PlacementDecision decision) {
  placement_decisions[obj_addr] = decision;
}

bool Context::has_placement_decision(addr_t obj_addr) {
  auto found_it = placement_decisions.find(obj_addr);
  return found_it != placement_decisions.end();
}

bool Context::check_compatible_placement_decision(
    addr_t obj_addr, PlacementDecision decision) const {
  auto found_it = placement_decisions.find(obj_addr);

  return found_it == placement_decisions.end() || found_it->second == decision;
}

bool Context::check_placement_decision(addr_t obj_addr,
                                       PlacementDecision decision) const {
  auto found_it = placement_decisions.find(obj_addr);

  return found_it != placement_decisions.end() && found_it->second == decision;
}

bool Context::check_if_can_be_ignored(const bdd::Node *node) const {
  auto id = node->get_id();
  return can_be_ignored_bdd_nodes.find(id) != can_be_ignored_bdd_nodes.end();
}

void Context::can_be_ignored(const bdd::Node *node) {
  auto id = node->get_id();
  can_be_ignored_bdd_nodes.insert(id);
}

} // namespace synapse