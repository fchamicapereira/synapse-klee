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
    ss << "Coalescing candidate:";
    ss << " map=" << candidate.map;
    ss << ", ";
    ss << "dchain=" << candidate.dchain;
    ss << ", ";
    ss << "vector_key=" << candidate.vector_key;
    ss << ", ";
    ss << "vectors_values=[";

    size_t i = 0;
    for (addr_t vector : candidate.vectors_values) {
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

static time_ns_t
exp_time_from_expire_items_single_map_time(const bdd::BDD *bdd,
                                           klee::ref<klee::Expr> time) {
  assert(time->getKind() == klee::Expr::Kind::Add);

  klee::ref<klee::Expr> lhs = time->getKid(0);
  klee::ref<klee::Expr> rhs = time->getKid(1);

  assert(lhs->getKind() == klee::Expr::Kind::Constant);

  const symbol_t &time_symbol = bdd->get_time();
  assert(kutil::solver_toolbox.are_exprs_always_equal(rhs, time_symbol.expr));

  uint64_t unsigned_exp_time = kutil::solver_toolbox.value_from_expr(lhs);
  time_ns_t exp_time = ~unsigned_exp_time + 1;

  return exp_time;
}

static std::optional<expiration_data_t>
build_expiration_data(const bdd::BDD *bdd) {
  std::optional<expiration_data_t> expiration_data;

  const bdd::Node *root = bdd->get_root();

  root->visit_nodes([&bdd, &expiration_data](const bdd::Node *node) {
    if (node->get_type() != bdd::NodeType::CALL) {
      return bdd::NodeVisitAction::VISIT_CHILDREN;
    }

    const bdd::Call *call_node = static_cast<const bdd::Call *>(node);
    const call_t &call = call_node->get_call();

    if (call.function_name != "expire_items_single_map") {
      return bdd::NodeVisitAction::VISIT_CHILDREN;
    }

    klee::ref<klee::Expr> time = call.args.at("time").expr;
    time_ns_t exp_time = exp_time_from_expire_items_single_map_time(bdd, time);

    symbols_t symbols = call_node->get_locally_generated_symbols();
    symbol_t number_of_freed_flows;
    bool found =
        get_symbol(symbols, "number_of_freed_flows", number_of_freed_flows);
    assert(found && "Symbol number_of_freed_flows not found");

    expiration_data_t data = {
        .expiration_time = exp_time,
        .number_of_freed_flows = number_of_freed_flows,
    };

    expiration_data = data;

    return bdd::NodeVisitAction::STOP;
  });

  return expiration_data;
}

Context::Context(const bdd::BDD *bdd,
                 const std::vector<const Target *> &targets,
                 const TargetType initial_target,
                 std::shared_ptr<Profiler> _profiler)
    : profiler(_profiler), profiler_mutations_allowed(false),
      expiration_data(build_expiration_data(bdd)), throughput_estimate_pps(0),
      throughput_speculation_pps(0) {
  for (const Target *target : targets) {
    target_ctxs[target->type] = target->ctx->clone();
    traffic_fraction_per_target[target->type] = 0.0;
  }

  traffic_fraction_per_target[initial_target] = 1.0;

  const std::vector<call_t> &init_calls = bdd->get_init();

  for (const call_t &call : init_calls) {
    if (call.function_name == "map_allocate") {
      klee::ref<klee::Expr> obj = call.args.at("map_out").out;
      addr_t addr = kutil::expr_addr_to_obj_addr(obj);
      bdd::map_config_t cfg = bdd::get_map_config(*bdd, addr);
      map_configs[addr] = cfg;

      map_coalescing_data_t candidate;
      if (get_map_coalescing_data(bdd, addr, candidate)) {
        coalescing_candidates.push_back(candidate);
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

  log_bdd_pre_processing(coalescing_candidates);
}

Context::Context(const Context &other)
    : profiler(other.profiler), profiler_mutations_allowed(false),
      map_configs(other.map_configs), vector_configs(other.vector_configs),
      dchain_configs(other.dchain_configs),
      sketch_configs(other.sketch_configs), cht_configs(other.cht_configs),
      coalescing_candidates(other.coalescing_candidates),
      expiration_data(other.expiration_data),
      placement_decisions(other.placement_decisions),
      traffic_fraction_per_target(other.traffic_fraction_per_target),
      constraints_per_node(other.constraints_per_node),
      throughput_estimate_pps(other.throughput_estimate_pps),
      throughput_speculation_pps(other.throughput_speculation_pps) {
  for (auto &target_ctx_pair : other.target_ctxs) {
    target_ctxs[target_ctx_pair.first] = target_ctx_pair.second->clone();
  }
}

Context::Context(Context &&other)
    : profiler(std::move(other.profiler)),
      profiler_mutations_allowed(other.profiler_mutations_allowed),
      map_configs(std::move(other.map_configs)),
      vector_configs(std::move(other.vector_configs)),
      dchain_configs(std::move(other.dchain_configs)),
      sketch_configs(std::move(other.sketch_configs)),
      cht_configs(std::move(other.cht_configs)),
      coalescing_candidates(std::move(other.coalescing_candidates)),
      expiration_data(std::move(other.expiration_data)),
      placement_decisions(std::move(other.placement_decisions)),
      target_ctxs(std::move(other.target_ctxs)),
      traffic_fraction_per_target(std::move(other.traffic_fraction_per_target)),
      constraints_per_node(std::move(other.constraints_per_node)),
      throughput_estimate_pps(std::move(other.throughput_estimate_pps)),
      throughput_speculation_pps(std::move(other.throughput_speculation_pps)) {}

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

  profiler = other.profiler;
  map_configs = other.map_configs;
  vector_configs = other.vector_configs;
  dchain_configs = other.dchain_configs;
  sketch_configs = other.sketch_configs;
  cht_configs = other.cht_configs;
  coalescing_candidates = other.coalescing_candidates;
  expiration_data = other.expiration_data;
  placement_decisions = other.placement_decisions;

  for (auto &target_ctx_pair : other.target_ctxs) {
    target_ctxs[target_ctx_pair.first] = target_ctx_pair.second->clone();
  }

  traffic_fraction_per_target = other.traffic_fraction_per_target;
  constraints_per_node = other.constraints_per_node;
  throughput_estimate_pps = other.throughput_estimate_pps;
  throughput_speculation_pps = other.throughput_speculation_pps;

  return *this;
}

const Profiler *Context::get_profiler() const { return profiler.get(); }

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
        candidate.vector_key == obj ||
        candidate.vectors_values.find(obj) != candidate.vectors_values.end()) {
      return candidate;
    }
  }

  return std::nullopt;
}

const std::optional<expiration_data_t> &Context::get_expiration_data() const {
  return expiration_data;
}

template <>
const tofino::TofinoContext *
Context::get_target_ctx<tofino::TofinoContext>() const {
  TargetType type = TargetType::Tofino;
  assert(target_ctxs.find(type) != target_ctxs.end());
  return static_cast<const tofino::TofinoContext *>(target_ctxs.at(type));
}

template <>
const tofino_cpu::TofinoCPUContext *
Context::get_target_ctx<tofino_cpu::TofinoCPUContext>() const {
  TargetType type = TargetType::TofinoCPU;
  assert(target_ctxs.find(type) != target_ctxs.end());
  return static_cast<const tofino_cpu::TofinoCPUContext *>(
      target_ctxs.at(type));
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
tofino_cpu::TofinoCPUContext *
Context::get_mutable_target_ctx<tofino_cpu::TofinoCPUContext>() {
  TargetType type = TargetType::TofinoCPU;
  assert(target_ctxs.find(type) != target_ctxs.end());
  return static_cast<tofino_cpu::TofinoCPUContext *>(target_ctxs.at(type));
}

template <>
x86::x86Context *Context::get_mutable_target_ctx<x86::x86Context>() {
  TargetType type = TargetType::x86;
  assert(target_ctxs.find(type) != target_ctxs.end());
  return static_cast<x86::x86Context *>(target_ctxs.at(type));
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

const std::unordered_map<TargetType, double> &
Context::get_traffic_fractions() const {
  return traffic_fraction_per_target;
}

void Context::update_traffic_fractions(const EPNode *new_node) {
  const Module *module = new_node->get_module();

  TargetType old_target = module->get_target();
  TargetType new_target = module->get_next_target();

  if (old_target == new_target) {
    return;
  }

  constraints_t constraints = get_node_constraints(new_node);
  std::optional<double> fraction = profiler->get_fraction(constraints);
  assert(fraction.has_value());

  update_traffic_fractions(old_target, new_target, *fraction);
}

void Context::update_traffic_fractions(TargetType old_target,
                                       TargetType new_target, double fraction) {
  double &old_target_fraction = traffic_fraction_per_target[old_target];
  double &new_target_fraction = traffic_fraction_per_target[new_target];

  old_target_fraction -= fraction;
  new_target_fraction += fraction;

  old_target_fraction = std::max(old_target_fraction, 0.0);
  old_target_fraction = std::min(old_target_fraction, 1.0);

  new_target_fraction = std::max(new_target_fraction, 0.0);
  new_target_fraction = std::min(new_target_fraction, 1.0);
}

void Context::update_constraints_per_node(ep_node_id_t node,
                                          const constraints_t &constraints) {
  assert(constraints_per_node.find(node) == constraints_per_node.end());
  constraints_per_node[node] = constraints;
}

constraints_t Context::get_node_constraints(const EPNode *node) const {
  assert(node);

  while (node) {
    ep_node_id_t node_id = node->get_id();
    auto found_it = constraints_per_node.find(node_id);

    if (found_it != constraints_per_node.end()) {
      return found_it->second;
    }

    node = node->get_prev();

    if (node) {
      assert(node->get_children().size() == 1 && "Ambiguous constraints");
    }
  }

  return {};
}

void Context::add_hit_rate_estimation(const constraints_t &constraints,
                                      klee::ref<klee::Expr> new_constraint,
                                      double estimation_rel) {
  allow_profiler_mutation();

  Log::dbg() << "Adding hit rate estimation " << estimation_rel << "\n";
  for (const klee::ref<klee::Expr> &constraint : constraints) {
    Log::dbg() << "   " << kutil::expr_to_string(constraint, true) << "\n";
  }

  profiler->insert_relative(constraints, new_constraint, estimation_rel);

  Log::dbg() << "\n";
  Log::dbg() << "Resulting Hit rate:\n";
  profiler->log_debug();
  Log::dbg() << "\n";
}

void Context::remove_hit_rate_node(const constraints_t &constraints) {
  allow_profiler_mutation();
  profiler->remove(constraints);
}

void Context::scale_profiler(const constraints_t &constraints, double factor) {
  allow_profiler_mutation();
  profiler->scale(constraints, factor);
}

void Context::allow_profiler_mutation() {
  if (profiler_mutations_allowed) {
    return;
  }

  Profiler *new_profiler = new Profiler(*profiler);
  profiler.reset(new_profiler);
  profiler_mutations_allowed = true;
}

std::ostream &operator<<(std::ostream &os, PlacementDecision decision) {
  switch (decision) {
  case PlacementDecision::Tofino_SimpleTable:
    os << "Tofino::SimpleTable";
    break;
  case PlacementDecision::Tofino_VectorRegister:
    os << "Tofino::Register";
    break;
  case PlacementDecision::Tofino_CachedTable:
    os << "Tofino::CachedTable";
    break;
  case PlacementDecision::TofinoCPU_Dchain:
    os << "TofinoCPU::Dchain";
    break;
  case PlacementDecision::TofinoCPU_Vector:
    os << "TofinoCPU::Vector";
    break;
  case PlacementDecision::TofinoCPU_Sketch:
    os << "TofinoCPU::Sketch";
    break;
  case PlacementDecision::TofinoCPU_Map:
    os << "TofinoCPU::Map";
    break;
  case PlacementDecision::TofinoCPU_Cht:
    os << "TofinoCPU::Cht";
    break;
  case PlacementDecision::x86_Map:
    os << "x86::Map";
    break;
  case PlacementDecision::x86_Vector:
    os << "x86::Vector";
    break;
  case PlacementDecision::x86_Dchain:
    os << "x86::Dchain";
    break;
  case PlacementDecision::x86_Sketch:
    os << "x86::Sketch";
    break;
  case PlacementDecision::x86_Cht:
    os << "x86::Cht";
    break;
  }
  return os;
}

void Context::log_debug() const {
  Log::dbg() << "~~~~~~~~~~~~~~~~~~~~~~~~ Context ~~~~~~~~~~~~~~~~~~~~~~~~\n";
  Log::dbg() << "Traffic fractions:\n";
  for (const auto &[target, fraction] : traffic_fraction_per_target) {
    Log::dbg() << "    " << target << ": " << fraction << "\n";
  }

  Log::dbg() << "Placement decisions: [\n";
  for (const auto &[obj, decision] : placement_decisions) {
    Log::dbg() << "    " << obj << ": " << decision << "\n";
  }
  Log::dbg() << "]\n";

  Log::dbg() << "Throughput estimate: " << throughput_estimate_pps << " pps\n";
  Log::dbg() << "Throughput speculation: " << throughput_speculation_pps
             << " pps\n";

  if (target_ctxs.find(TargetType::Tofino) != target_ctxs.end()) {
    const tofino::TofinoContext *tofino_ctx =
        get_target_ctx<tofino::TofinoContext>();
    const tofino::TNA &tna = tofino_ctx->get_tna();
    tna.log_debug_placement();
    tna.log_debug_perf_oracle();
  }

  Log::dbg() << "\n";
  Log::dbg() << "~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~\n";
}

} // namespace synapse