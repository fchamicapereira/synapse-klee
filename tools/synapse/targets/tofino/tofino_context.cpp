#include "tofino.h"
#include "tofino_context.h"

#include "../../execution_plan/execution_plan.h"
#include "../module.h"

#include "../../visualizers/ep_visualizer.h"

#include <algorithm>

namespace synapse {
namespace tofino {

TofinoContext::TofinoContext(TNAVersion _version, const Profiler *profiler)
    : tna(_version, profiler->get_avg_pkt_bytes()) {}

TofinoContext::TofinoContext(const TofinoContext &other) : tna(other.tna) {
  for (const auto &kv : other.obj_to_ds) {
    std::vector<DS *> new_ds;
    for (const auto &ds : kv.second) {
      DS *clone = ds->clone();
      new_ds.push_back(clone);
      id_to_ds[clone->id] = clone;
    }
    obj_to_ds[kv.first] = new_ds;
  }
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
}

bool TofinoContext::has_ds(addr_t addr) const {
  return obj_to_ds.find(addr) != obj_to_ds.end();
}

const std::vector<DS *> &TofinoContext::get_ds(addr_t addr) const {
  auto found_it = obj_to_ds.find(addr);
  assert(found_it != obj_to_ds.end() && "Data structure not found");
  return found_it->second;
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

  tna.parser.log_debug();
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

  tna.parser.log_debug();
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

  tna.parser.log_debug();
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

  tna.parser.log_debug();
}

static const EPNode *
get_ep_node_leaf_from_future_bdd_node(const EP *ep, const bdd::Node *node) {
  const std::vector<EPLeaf> &leaves = ep->get_leaves();

  while (node) {
    for (const EPLeaf &leaf : leaves) {
      if (leaf.next == node) {
        return leaf.node;
      }
    }

    node = node->get_prev();
  }

  return nullptr;
}

std::unordered_set<DS_ID>
TofinoContext::get_stateful_deps(const EP *ep, const bdd::Node *node) const {
  std::unordered_set<DS_ID> deps;

  const EPNode *ep_node = get_ep_node_leaf_from_future_bdd_node(ep, node);

  if (!ep_node) {
    return deps;
  }

  while (ep_node) {
    const Module *module = ep_node->get_module();

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

    ep_node = ep_node->get_prev();
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
    Log::dbg() << "[" << target << "] Cannot place ds " << ds->id << " ("
               << status << ")\n";
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
    Log::dbg() << "  DS:\n";
    for (const auto &ds_list : ds) {
      for (const DS *ds : ds_list) {
        Log::dbg() << "   * " << ds->id << "\n";
      }
    }
    Log::dbg() << "  Deps:\n";
    for (DS_ID dep : deps) {
      Log::dbg() << "   * " << dep << "\n";
    }
  }

  return status == PlacementStatus::SUCCESS;
}

void TofinoContext::add_recirculated_traffic(
    int port, int port_recirculations, double fraction,
    std::optional<int> prev_recirc_port) {
  PerfOracle &oracle = tna.get_mutable_perf_oracle();
  oracle.add_recirculated_traffic(port, port_recirculations, fraction,
                                  prev_recirc_port);
}

uint64_t TofinoContext::estimate_throughput_pps() const {
  const PerfOracle &oracle = tna.get_perf_oracle();
  return oracle.estimate_throughput_pps();
}

} // namespace tofino
} // namespace synapse