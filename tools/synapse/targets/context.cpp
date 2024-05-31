#include "context.h"
#include "target.h"

namespace synapse {

Context::Context(const std::vector<const Target *> &targets) {
  for (const Target *target : targets) {
    target_ctxs[target->type] = target->ctx->clone();
  }
}

Context::Context(const Context &other)
    : map_configs(other.map_configs), vector_configs(other.vector_configs),
      dchain_configs(other.dchain_configs),
      sketch_configs(other.sketch_configs), cht_configs(other.cht_configs),
      reorder_ops(other.reorder_ops),
      placement_decisions(other.placement_decisions),
      can_be_ignored_bdd_nodes(other.can_be_ignored_bdd_nodes),
      expiration_data(other.expiration_data) {
  for (auto &target_ctx_pair : other.target_ctxs) {
    target_ctxs[target_ctx_pair.first] = target_ctx_pair.second->clone();
  }
}

Context::Context(Context &&other)
    : map_configs(std::move(other.map_configs)),
      vector_configs(std::move(other.vector_configs)),
      dchain_configs(std::move(other.dchain_configs)),
      sketch_configs(std::move(other.sketch_configs)),
      cht_configs(std::move(other.cht_configs)),
      reorder_ops(std::move(other.reorder_ops)),
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

  map_configs = other.map_configs;
  vector_configs = other.vector_configs;
  dchain_configs = other.dchain_configs;
  sketch_configs = other.sketch_configs;
  cht_configs = other.cht_configs;

  reorder_ops = other.reorder_ops;
  placement_decisions = other.placement_decisions;
  can_be_ignored_bdd_nodes = other.can_be_ignored_bdd_nodes;
  expiration_data = other.expiration_data;

  for (auto &target_ctx_pair : other.target_ctxs) {
    target_ctxs[target_ctx_pair.first] = target_ctx_pair.second->clone();
  }

  return *this;
}

bool Context::has_map_config(addr_t addr) const {
  return map_configs.find(addr) != map_configs.end();
}

bool Context::has_vector_config(addr_t addr) const {
  return vector_configs.find(addr) != vector_configs.end();
}

bool Context::has_dchain_config(addr_t addr) const {
  return dchain_configs.find(addr) != dchain_configs.end();
}

bool Context::has_sketch_config(addr_t addr) const {
  return sketch_configs.find(addr) != sketch_configs.end();
}

bool Context::has_cht_config(addr_t addr) const {
  return cht_configs.find(addr) != cht_configs.end();
}

const bdd::map_config_t &Context::get_map_config(addr_t addr) const {
  assert(has_map_config(addr));
  return map_configs.at(addr);
}

const bdd::vector_config_t &Context::get_vector_config(addr_t addr) const {
  assert(has_vector_config(addr));
  return vector_configs.at(addr);
}

const bdd::dchain_config_t &Context::get_dchain_config(addr_t addr) const {
  assert(has_dchain_config(addr));
  return dchain_configs.at(addr);
}

const bdd::sketch_config_t &Context::get_sketch_config(addr_t addr) const {
  assert(has_sketch_config(addr));
  return sketch_configs.at(addr);
}

const bdd::cht_config_t &Context::get_cht_config(addr_t addr) const {
  assert(has_cht_config(addr));
  return cht_configs.at(addr);
}

void Context::save_map_config(addr_t addr, bdd::map_config_t cfg) {
  map_configs[addr] = cfg;
}

void Context::save_vector_config(addr_t addr, bdd::vector_config_t cfg) {
  vector_configs[addr] = cfg;
}

void Context::save_dchain_config(addr_t addr, bdd::dchain_config_t cfg) {
  dchain_configs[addr] = cfg;
}

void Context::save_sketch_config(addr_t addr, bdd::sketch_config_t cfg) {
  sketch_configs[addr] = cfg;
}

void Context::save_cht_config(addr_t addr, bdd::cht_config_t cfg) {
  cht_configs[addr] = cfg;
}

void Context::set_expiration_data(const expiration_data_t &_expiration_data) {
  expiration_data = _expiration_data;
}

const expiration_data_t &Context::get_expiration_data() const {
  return expiration_data;
}

const TargetContext *Context::get_target_context(TargetType type) const {
  assert(target_ctxs.find(type) != target_ctxs.end());
  return target_ctxs.at(type);
}

TargetContext *Context::get_mutable_target_context(TargetType type) {
  assert(target_ctxs.find(type) != target_ctxs.end());
  return target_ctxs.at(type);
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