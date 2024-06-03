#include "context.h"
#include "target.h"
#include "targets.h"

#include "klee-util.h"

namespace synapse {

static void log_bdd_pre_processing(
    const std::vector<map_coalescing_data_t> &coalescing_candidates) {
  Log::dbg() << "***** BDD pre-processing: *****\n";
  for (const map_coalescing_data_t &candidate : coalescing_candidates) {
    std::stringstream ss;
    ss << "Coalescing candidate: map=" << candidate.map
       << ", dchain=" << candidate.dchain << ", vectors=[";

    size_t i = 0;
    for (addr_t vector : candidate.vectors) {
      if (i++ > 0) {
        ss << ", ";
      }
      ss << vector;
    }

    ss << "]\n";
    Log::dbg() << ss.str();
  }
  Log::dbg() << "*******************************\n";
}

Context::Context(const bdd::BDD *bdd,
                 const std::vector<const Target *> &targets) {
  for (const Target *target : targets) {
    target_ctxs[target->type] = target->ctx->clone();
  }

  const std::vector<call_t> &init_calls = bdd->get_init();

  for (const call_t &call : init_calls) {
    if (call.function_name == "map_allocate") {
      klee::ref<klee::Expr> obj = call.args.at("map_out").out;
      addr_t addr = kutil::expr_addr_to_obj_addr(obj);
      bdd::map_config_t cfg = bdd::get_map_config(*bdd, addr);
      map_configs[addr] = cfg;

      std::optional<map_coalescing_data_t> candidate =
          get_map_coalescing_data(bdd, addr);
      if (candidate.has_value()) {
        coalescing_candidates.push_back(*candidate);
      }

      continue;
    }

    if (call.function_name == "vector_allocate") {
      klee::ref<klee::Expr> obj = call.args.at("vector_out").out;
      addr_t addr = kutil::expr_addr_to_obj_addr(obj);
      bdd::vector_config_t cfg = bdd::get_vector_config(*bdd, addr);
      vector_configs[addr] = cfg;
      continue;
    }

    if (call.function_name == "dchain_allocate") {
      klee::ref<klee::Expr> obj = call.args.at("chain_out").out;
      addr_t addr = kutil::expr_addr_to_obj_addr(obj);
      bdd::dchain_config_t cfg = bdd::get_dchain_config(*bdd, addr);
      dchain_configs[addr] = cfg;
      continue;
    }

    if (call.function_name == "sketch_allocate") {
      klee::ref<klee::Expr> obj = call.args.at("sketch_out").out;
      addr_t addr = kutil::expr_addr_to_obj_addr(obj);
      bdd::sketch_config_t cfg = bdd::get_sketch_config(*bdd, addr);
      sketch_configs[addr] = cfg;
      continue;
    }

    if (call.function_name == "cht_fill_cht") {
      klee::ref<klee::Expr> obj = call.args.at("cht").expr;
      addr_t addr = kutil::expr_addr_to_obj_addr(obj);
      bdd::cht_config_t cfg = bdd::get_cht_config(*bdd, addr);
      cht_configs[addr] = cfg;
      continue;
    }

    assert(false && "Unknown init call");
  }

  // FIXME: full up expiration_data

  log_bdd_pre_processing(coalescing_candidates);
}

Context::Context(const Context &other)
    : map_configs(other.map_configs), vector_configs(other.vector_configs),
      dchain_configs(other.dchain_configs),
      sketch_configs(other.sketch_configs), cht_configs(other.cht_configs),
      coalescing_candidates(other.coalescing_candidates),
      expiration_data(other.expiration_data), reorder_ops(other.reorder_ops),
      placement_decisions(other.placement_decisions) {
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
      coalescing_candidates(std::move(other.coalescing_candidates)),
      expiration_data(std::move(other.expiration_data)),
      reorder_ops(std::move(other.reorder_ops)),
      placement_decisions(std::move(other.placement_decisions)),
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
  coalescing_candidates = other.coalescing_candidates;
  expiration_data = other.expiration_data;

  reorder_ops = other.reorder_ops;
  placement_decisions = other.placement_decisions;

  for (auto &target_ctx_pair : other.target_ctxs) {
    target_ctxs[target_ctx_pair.first] = target_ctx_pair.second->clone();
  }

  return *this;
}

const bdd::map_config_t &Context::get_map_config(addr_t addr) const {
  assert(map_configs.find(addr) != map_configs.end());
  return map_configs.at(addr);
}

const bdd::vector_config_t &Context::get_vector_config(addr_t addr) const {
  assert(vector_configs.find(addr) != vector_configs.end());
  return vector_configs.at(addr);
}

const bdd::dchain_config_t &Context::get_dchain_config(addr_t addr) const {
  assert(dchain_configs.find(addr) != dchain_configs.end());
  return dchain_configs.at(addr);
}

const bdd::sketch_config_t &Context::get_sketch_config(addr_t addr) const {
  assert(sketch_configs.find(addr) != sketch_configs.end());
  return sketch_configs.at(addr);
}

const bdd::cht_config_t &Context::get_cht_config(addr_t addr) const {
  assert(cht_configs.find(addr) != cht_configs.end());
  return cht_configs.at(addr);
}

std::optional<map_coalescing_data_t>
Context::get_coalescing_data(addr_t obj) const {
  for (const map_coalescing_data_t &candidate : coalescing_candidates) {
    if (candidate.map == obj || candidate.dchain == obj ||
        candidate.vectors.find(obj) != candidate.vectors.end()) {
      return candidate;
    }
  }

  return std::nullopt;
}

const expiration_data_t &Context::get_expiration_data() const {
  return expiration_data;
}

// template <class TCtx> const TCtx *Context::get_target_ctx() const {
//   return nullptr;
// }
// template <class TCtx> TCtx *Context::get_mutable_target_ctx() {
//   return nullptr;
// }

template <>
const tofino::TofinoContext *
Context::get_target_ctx<tofino::TofinoContext>() const {
  TargetType type = TargetType::Tofino;
  assert(target_ctxs.find(type) != target_ctxs.end());
  return static_cast<const tofino::TofinoContext *>(target_ctxs.at(type));
}

template <>
const x86::x86Context *Context::get_target_ctx<x86::x86Context>() const {
  TargetType type = TargetType::x86;
  assert(target_ctxs.find(type) != target_ctxs.end());
  return static_cast<const x86::x86Context *>(target_ctxs.at(type));
}

template <>
tofino::TofinoContext *
Context::get_mutable_target_ctx<tofino::TofinoContext>() {
  TargetType type = TargetType::Tofino;
  assert(target_ctxs.find(type) != target_ctxs.end());
  return static_cast<tofino::TofinoContext *>(target_ctxs.at(type));
}

template <>
x86::x86Context *Context::get_mutable_target_ctx<x86::x86Context>() {
  TargetType type = TargetType::x86;
  assert(target_ctxs.find(type) != target_ctxs.end());
  return static_cast<x86::x86Context *>(target_ctxs.at(type));
}

void Context::add_reorder_op(const bdd::reorder_op_t &op) {
  reorder_ops.push_back(op);
}

void Context::save_placement(addr_t obj, PlacementDecision decision) {
  assert(can_place(obj, decision) && "Incompatible placement decision");
  placement_decisions[obj] = decision;
}

bool Context::has_placement(addr_t obj) const {
  return placement_decisions.find(obj) != placement_decisions.end();
}

bool Context::check_placement(addr_t obj, PlacementDecision decision) const {
  auto found_it = placement_decisions.find(obj);
  return found_it != placement_decisions.end() && found_it->second == decision;
}

bool Context::can_place(addr_t obj, PlacementDecision decision) const {
  auto found_it = placement_decisions.find(obj);
  return found_it == placement_decisions.end() || found_it->second == decision;
}

const std::unordered_map<addr_t, PlacementDecision> &
Context::get_placements() const {
  return placement_decisions;
}

std::ostream &operator<<(std::ostream &os, PlacementDecision decision) {
  switch (decision) {
  case PlacementDecision::TofinoSimpleTable:
    os << "Tofino::SimpleTable";
    break;
  case PlacementDecision::TofinoCPUDchain:
    os << "TofinoCPU::Dchain";
    break;
  case PlacementDecision::TofinoCPUVector:
    os << "TofinoCPU::Vector";
    break;
  case PlacementDecision::TofinoCPUSketch:
    os << "TofinoCPU::Sketch";
    break;
  case PlacementDecision::TofinoCPUMap:
    os << "TofinoCPU::Map";
    break;
  case PlacementDecision::TofinoCPUCht:
    os << "TofinoCPU::Cht";
    break;
  case PlacementDecision::x86Map:
    os << "x86::Map";
    break;
  case PlacementDecision::x86Vector:
    os << "x86::Vector";
    break;
  case PlacementDecision::x86Dchain:
    os << "x86::Dchain";
    break;
  case PlacementDecision::x86Sketch:
    os << "x86::Sketch";
    break;
  case PlacementDecision::x86Cht:
    os << "x86::Cht";
    break;
  }
  return os;
}

} // namespace synapse