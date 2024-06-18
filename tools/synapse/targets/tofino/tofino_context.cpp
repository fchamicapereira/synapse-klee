#include "tofino.h"
#include "tofino_context.h"

#include "../../execution_plan/execution_plan.h"
#include "../module.h"

#include "../../visualizers/ep_visualizer.h"

#include <algorithm>

namespace synapse {
namespace tofino {

TofinoContext::TofinoContext(TNAVersion _version)
    : tna(_version),
      recirc_ports(tna.get_properties().total_recirculation_ports),
      recirc_fraction_per_recirc_port(new float[recirc_ports]),
      recirc_surplus_per_recirc_port(new float[recirc_ports]) {
  for (int i = 0; i < recirc_ports; i++) {
    recirc_fraction_per_recirc_port[i] = 0;
    recirc_surplus_per_recirc_port[i] = 0;
  }
}

TofinoContext::TofinoContext(const TofinoContext &other)
    : tna(other.tna), recirc_ports(other.recirc_ports),
      recirc_fraction_per_recirc_port(new float[recirc_ports]),
      recirc_surplus_per_recirc_port(new float[recirc_ports]) {
  for (const auto &kv : other.obj_to_ds) {
    std::vector<DS *> new_ds;
    for (const auto &ds : kv.second) {
      DS *clone = ds->clone();
      new_ds.push_back(clone);
      id_to_ds[clone->id] = clone;
    }
    obj_to_ds[kv.first] = new_ds;
  }

  std::copy(recirc_fraction_per_recirc_port,
            recirc_fraction_per_recirc_port + recirc_ports,
            other.recirc_fraction_per_recirc_port);

  std::copy(recirc_surplus_per_recirc_port,
            recirc_surplus_per_recirc_port + recirc_ports,
            other.recirc_surplus_per_recirc_port);
}

TofinoContext::~TofinoContext() {
  for (auto &kv : obj_to_ds) {
    for (auto &ds : kv.second) {
      delete ds;
    }
    kv.second.clear();
  }
  obj_to_ds.clear();
  id_to_ds.clear();

  delete[] recirc_fraction_per_recirc_port;
  delete[] recirc_surplus_per_recirc_port;
}

const std::vector<DS *> &TofinoContext::get_ds(addr_t addr) const {
  return obj_to_ds.at(addr);
}

void TofinoContext::save_ds(addr_t addr, DS *ds) {
  assert(id_to_ds.find(ds->id) == id_to_ds.end() &&
         "Duplicate data structure ID");
  obj_to_ds[addr].push_back(ds);
  id_to_ds[ds->id] = ds;
}

const DS *TofinoContext::get_ds_from_id(DS_ID id) const {
  auto it = id_to_ds.find(id);
  if (it == id_to_ds.end()) {
    return nullptr;
  }
  return it->second;
}

static const bdd::Node *
get_last_parser_state_op(const EP *ep, std::optional<bool> &direction) {
  const EPLeaf *leaf = ep->get_active_leaf();

  const EPNode *node = leaf->node;
  const EPNode *next = nullptr;

  while (node) {
    const Module *module = node->get_module();
    assert(module && "Module not found");

    if (module->get_type() == ModuleType::Tofino_ParserCondition) {
      assert(next && next->get_module());
      const Module *next_module = next->get_module();

      if (next_module->get_type() == ModuleType::Tofino_Then) {
        direction = true;
      } else {
        direction = false;
      }

      return module->get_node();
    }

    if (module->get_type() == ModuleType::Tofino_ParserExtraction) {
      return module->get_node();
    }

    next = node;
    node = node->get_prev();
  }

  return nullptr;
}

void TofinoContext::parser_select(const EP *ep, const bdd::Node *node,
                                  klee::ref<klee::Expr> field,
                                  const std::vector<int> &values) {
  bdd::node_id_t id = node->get_id();

  std::optional<bool> direction;
  const bdd::Node *last_op = get_last_parser_state_op(ep, direction);

  if (!last_op) {
    // No leaf node found, add the initial parser state.
    tna.parser.add_select(id, field, values);
    return;
  }

  bdd::node_id_t leaf_id = last_op->get_id();
  tna.parser.add_select(leaf_id, id, field, values, direction);
}

void TofinoContext::parser_transition(const EP *ep, const bdd::Node *node,
                                      klee::ref<klee::Expr> hdr) {
  bdd::node_id_t id = node->get_id();

  std::optional<bool> direction;
  const bdd::Node *last_op = get_last_parser_state_op(ep, direction);

  if (!last_op) {
    // No leaf node found, add the initial parser state.
    tna.parser.add_extract(id, hdr);
    return;
  }

  bdd::node_id_t leaf_id = last_op->get_id();
  tna.parser.add_extract(leaf_id, id, hdr, direction);
}

void TofinoContext::parser_accept(const EP *ep, const bdd::Node *node) {
  bdd::node_id_t id = node->get_id();

  std::optional<bool> direction;
  const bdd::Node *last_op = get_last_parser_state_op(ep, direction);

  if (!last_op) {
    // No leaf node found, add the initial parser state.
    tna.parser.accept(id);
  } else {
    bdd::node_id_t leaf_id = last_op->get_id();
    tna.parser.accept(leaf_id, id, direction);
  }
}

void TofinoContext::parser_reject(const EP *ep, const bdd::Node *node) {
  bdd::node_id_t id = node->get_id();

  std::optional<bool> direction;
  const bdd::Node *last_op = get_last_parser_state_op(ep, direction);

  if (!last_op) {
    // No leaf node found, add the initial parser state.
    tna.parser.reject(id);
  } else {
    bdd::node_id_t leaf_id = last_op->get_id();
    tna.parser.reject(leaf_id, id, direction);
  }
}

std::unordered_set<DS_ID> TofinoContext::get_stateful_deps(const EP *ep) const {
  std::unordered_set<DS_ID> deps;

  const EPLeaf *active_leaf = ep->get_active_leaf();
  if (!active_leaf || !active_leaf->node) {
    return deps;
  }

  const EPNode *node = active_leaf->node;
  while (node) {
    const Module *module = node->get_module();

    if (module->get_target() != TargetType::Tofino) {
      break;
    }

    if (module->get_type() == ModuleType::Tofino_Recirculate) {
      break;
    }

    const TofinoModule *tofino_module =
        static_cast<const TofinoModule *>(module);
    const std::unordered_set<DS_ID> &generated_ds =
        tofino_module->get_generated_ds();
    deps.insert(generated_ds.begin(), generated_ds.end());

    node = node->get_prev();
  }

  return deps;
}

void TofinoContext::place(EP *ep, addr_t obj, DS *ds,
                          const std::unordered_set<DS_ID> &deps) {
  save_ds(obj, ds);
  tna.place(ds, deps);
}

void TofinoContext::place_many(EP *ep, addr_t obj,
                               const std::vector<std::unordered_set<DS *>> &ds,
                               const std::unordered_set<DS_ID> &_deps) {
  std::unordered_set<DS_ID> deps = _deps;

  for (const std::unordered_set<DS *> &ds_list : ds) {
    for (DS *ds : ds_list) {
      save_ds(obj, ds);
    }
  }

  tna.place_many(ds, deps);
}

bool TofinoContext::check_placement(
    const EP *ep, const DS *ds, const std::unordered_set<DS_ID> &deps) const {
  PlacementStatus status = tna.can_place(ds, deps);

  if (status != PlacementStatus::SUCCESS) {
    TargetType target = ep->get_current_platform();
    Log::dbg() << "[" << target << "] Cannot place obj (" << status << ")\n";
  }

  return status == PlacementStatus::SUCCESS;
}

bool TofinoContext::check_many_placements(
    const EP *ep, const std::vector<std::unordered_set<DS *>> &ds,
    const std::unordered_set<DS_ID> &deps) const {
  PlacementStatus status = tna.can_place_many(ds, deps);

  if (status != PlacementStatus::SUCCESS) {
    TargetType target = ep->get_current_platform();
    Log::dbg() << "[" << target << "] Cannot place objs (" << status << ")\n";
  }

  return status == PlacementStatus::SUCCESS;
}

int TofinoContext::estimate_throughput_kpps() const {
  const TNAProperties &properties = tna.get_properties();

  float r = 0;
  float Ts = properties.port_capacity_pps * properties.total_ports;
  float throughput_estimation_pps = 0;

  for (int port = 0; port < recirc_ports; port++) {
    float ri = recirc_fraction_per_recirc_port[port];
    float rsi = recirc_surplus_per_recirc_port[port];

    r += ri;

    float r_load = Ts * ri;
    float r_cap = properties.port_capacity_pps / (rsi + 1);

    throughput_estimation_pps += std::min(r_load, r_cap);
  }

  throughput_estimation_pps += Ts * (1 - r);

  return throughput_estimation_pps / 1'000;
}

} // namespace tofino
} // namespace synapse