#include "tofino_context.h"

#include "../../execution_plan/execution_plan.h"
#include "../module.h"

#include "../../visualizers/ep_visualizer.h"

namespace synapse {
namespace tofino {

static std::unordered_map<TNAProperty, int>
tna_properties_from_version(TNAVersion version) {
  std::unordered_map<TNAProperty, int> properties;

  switch (version) {
  case TNAVersion::TNA1:
    properties = {
        {TNAProperty::MAX_PACKET_BYTES_IN_CONDITION, 4},
        {TNAProperty::STAGES, 12},
        {TNAProperty::SRAM_PER_STAGE_BITS, 128 * 1024 * 80},
        {TNAProperty::TCAM_PER_STAGE_BITS, 44 * 512 * 24},
        {TNAProperty::MAX_LOGICAL_TCAM_TABLES, 8},
        {TNAProperty::MAX_LOGICAL_SRAM_AND_TCAM_TABLES, 16},
        {TNAProperty::PHV_SIZE_BITS, 4000},
        {TNAProperty::PHV_8BIT_CONTAINERS, 64},
        {TNAProperty::PHV_16BIT_CONTAINERS, 96},
        {TNAProperty::PHV_32BIT_CONTAINERS, 64},
        {TNAProperty::PACKET_BUFFER_SIZE_BITS, 20e6 * 8},
        {TNAProperty::EXACT_MATCH_XBAR_BITS, 128 * 8},
        {TNAProperty::MAX_EXACT_MATCH_KEYS, 16},
        {TNAProperty::TERNARY_MATCH_XBAR_BITS, 66 * 8},
        {TNAProperty::MAX_TERNARY_MATCH_KEYS, 8},
    };
    break;
  case TNAVersion::TNA2:
    properties = {
        {TNAProperty::MAX_PACKET_BYTES_IN_CONDITION, 4},
        {TNAProperty::STAGES, 20},
        {TNAProperty::SRAM_PER_STAGE_BITS, 128 * 1024 * 80},
        {TNAProperty::TCAM_PER_STAGE_BITS, 44 * 512 * 24},
        {TNAProperty::MAX_LOGICAL_TCAM_TABLES, 8},
        {TNAProperty::MAX_LOGICAL_SRAM_AND_TCAM_TABLES, 16},
        {TNAProperty::PHV_SIZE_BITS, 5000},
        {TNAProperty::PHV_8BIT_CONTAINERS, 80},
        {TNAProperty::PHV_16BIT_CONTAINERS, 120},
        {TNAProperty::PHV_32BIT_CONTAINERS, 80},
        {TNAProperty::PACKET_BUFFER_SIZE_BITS, 64e6 * 8},
        {TNAProperty::EXACT_MATCH_XBAR_BITS, 128 * 8},
        {TNAProperty::MAX_EXACT_MATCH_KEYS, 16},
        {TNAProperty::TERNARY_MATCH_XBAR_BITS, 66 * 8},
        {TNAProperty::MAX_TERNARY_MATCH_KEYS, 8},
    };
    break;
  }

  return properties;
}

TofinoContext::TofinoContext(TNAVersion _version)
    : version(_version), properties(tna_properties_from_version(_version)) {}

TofinoContext::TofinoContext(const TofinoContext &other)
    : version(other.version), properties(other.properties), ids(other.ids),
      parser(other.parser) {
  for (const auto &kv : other.data_structures) {
    std::vector<DataStructure *> new_data_structures;
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

const std::vector<DataStructure *> &
TofinoContext::get_data_structures(addr_t addr) const {
  return data_structures.at(addr);
}

void TofinoContext::add_data_structure(addr_t addr, DataStructure *ds) {
  assert(ids.find(ds->id) == ids.end() && "Duplicate data structure ID");
  data_structures[addr].push_back(ds);
}

bool TofinoContext::condition_meets_phv_limit(
    klee::ref<klee::Expr> expr) const {
  kutil::SymbolRetriever retriever;
  retriever.visit(expr);

  const std::vector<klee::ref<klee::ReadExpr>> &chunks =
      retriever.get_retrieved_packet_chunks();

  return static_cast<int>(chunks.size()) <=
         properties.at(TNAProperty::MAX_PACKET_BYTES_IN_CONDITION);
}

const Table *TofinoContext::get_table(int tid) const {
  for (const auto &kv : data_structures) {
    for (const DataStructure *ds : kv.second) {
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
    parser.add_select(id, field, values);
    return;
  }

  bdd::node_id_t leaf_id = last_op->get_id();
  parser.add_select(leaf_id, id, field, values, direction);
}

void TofinoContext::parser_transition(const EP *ep, const bdd::Node *node,
                                      klee::ref<klee::Expr> hdr) {
  bdd::node_id_t id = node->get_id();

  std::optional<bool> direction;
  const bdd::Node *last_op = get_last_parser_state_op(ep, direction);

  if (!last_op) {
    // No leaf node found, add the initial parser state.
    parser.add_extract(id, hdr);
    return;
  }

  bdd::node_id_t leaf_id = last_op->get_id();
  parser.add_extract(leaf_id, id, hdr, direction);
}

void TofinoContext::parser_accept(const EP *ep, const bdd::Node *node) {
  bdd::node_id_t id = node->get_id();

  std::optional<bool> direction;
  const bdd::Node *last_op = get_last_parser_state_op(ep, direction);
  assert(last_op && "Last borrow node not found");

  bdd::node_id_t leaf_id = last_op->get_id();
  parser.accept(leaf_id, id, direction);
}

void TofinoContext::parser_reject(const EP *ep, const bdd::Node *node) {
  bdd::node_id_t id = node->get_id();

  std::optional<bool> direction;
  const bdd::Node *last_op = get_last_parser_state_op(ep, direction);
  assert(last_op && "Last borrow node not found");

  bdd::node_id_t leaf_id = last_op->get_id();
  parser.reject(leaf_id, id, direction);
}

} // namespace tofino
} // namespace synapse