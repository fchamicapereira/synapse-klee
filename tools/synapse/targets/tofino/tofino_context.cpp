#include "tofino.h"
#include "tofino_context.h"

#include "../../execution_plan/execution_plan.h"
#include "../module.h"

#include "../../visualizers/ep_visualizer.h"

namespace synapse {
namespace tofino {

TofinoContext::TofinoContext(TNAVersion _version) : tna(_version) {}

TofinoContext::TofinoContext(const TofinoContext &other)
    : tna(other.tna), ids(other.ids) {
  for (const auto &kv : other.data_structures) {
    std::vector<DS *> new_data_structures;
    for (const auto &ds : kv.second) {
      new_data_structures.push_back(ds->clone());
    }
    data_structures[kv.first] = new_data_structures;
  }
}

TofinoContext::~TofinoContext() {
  for (auto &kv : data_structures) {
    for (auto &ds : kv.second) {
      delete ds;
    }
    kv.second.clear();
  }
  data_structures.clear();
}

const std::vector<DS *> &TofinoContext::get_ds(addr_t addr) const {
  return data_structures.at(addr);
}

void TofinoContext::save_ds(addr_t addr, DS *ds) {
  assert(ids.find(ds->id) == ids.end() && "Duplicate data structure ID");
  data_structures[addr].push_back(ds);
}

const Table *TofinoContext::get_table(int tid) const {
  for (const auto &kv : data_structures) {
    for (const DS *ds : kv.second) {
      if (ds->type == DSType::SIMPLE_TABLE) {
        const Table *table = static_cast<const Table *>(ds);
        if (table->id == tid) {
          return table;
        }
      }
    }
  }

  return nullptr;
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
  assert(last_op && "Last borrow node not found");

  bdd::node_id_t leaf_id = last_op->get_id();
  tna.parser.accept(leaf_id, id, direction);
}

void TofinoContext::parser_reject(const EP *ep, const bdd::Node *node) {
  bdd::node_id_t id = node->get_id();

  std::optional<bool> direction;
  const bdd::Node *last_op = get_last_parser_state_op(ep, direction);
  assert(last_op && "Last borrow node not found");

  bdd::node_id_t leaf_id = last_op->get_id();
  tna.parser.reject(leaf_id, id, direction);
}

std::unordered_set<const DS *> TofinoContext::get_prev_ds(const EP *ep,
                                                          DSType type) const {
  std::unordered_set<const DS *> prev_ds;
  const EPLeaf *active_leaf = ep->get_active_leaf();

  if (!active_leaf || !active_leaf->node) {
    return prev_ds;
  }

  const EPNode *node = active_leaf->node;
  while (node) {
    const Module *module = node->get_module();

    if (module->get_target() != TargetType::Tofino) {
      break;
    }

    switch (type) {
    case DSType::SIMPLE_TABLE: {
      if (module->get_type() == ModuleType::Tofino_SimpleTableLookup) {
        const SimpleTableLookup *table_lookup =
            static_cast<const SimpleTableLookup *>(module);
        int table_id = table_lookup->get_table_id();
        const Table *table = get_table(table_id);
        assert(table && "Table not found");
        prev_ds.insert(table);
      }
    } break;
    }

    node = node->get_prev();
  }

  return prev_ds;
}

std::unordered_set<DS_ID>
TofinoContext::get_table_dependencies(const EP *ep) const {
  std::unordered_set<const DS *> prev_tables =
      get_prev_ds(ep, DSType::SIMPLE_TABLE);

  std::unordered_set<DS_ID> dependencies;
  for (const DS *ds : prev_tables) {
    dependencies.insert(ds->id);
  }

  return dependencies;
}

void TofinoContext::save_table(EP *ep, addr_t obj, Table *table,
                               const std::unordered_set<DS_ID> &dependencies) {
  save_ds(obj, table);
  tna.place_table(table, dependencies);
  tna.log_debug_placement();
}

bool TofinoContext::check_table_placement(
    const EP *ep, const Table *table,
    const std::unordered_set<DS_ID> &dependencies) const {
  PlacementStatus status = tna.can_place_table(table, dependencies);

  if (status != PlacementStatus::SUCCESS) {
    TargetType target = ep->get_current_platform();
    Log::dbg() << "[" << target << "] Cannot place table (" << status << ")\n";
    table->log_debug();
  }

  return status == PlacementStatus::SUCCESS;
}

} // namespace tofino
} // namespace synapse