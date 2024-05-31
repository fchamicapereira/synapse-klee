#include "tna_arch.h"

#include "../../../execution_plan/execution_plan.h"
#include "../../module.h"

#include "../../../visualizers/ep_visualizer.h"

namespace synapse {
namespace tofino {

TNA::TNA(TNAVersion version) {
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
}

bool TNA::condition_meets_phv_limit(klee::ref<klee::Expr> expr) const {
  kutil::SymbolRetriever retriever;
  retriever.visit(expr);

  const std::vector<klee::ref<klee::ReadExpr>> &chunks =
      retriever.get_retrieved_packet_chunks();

  return static_cast<int>(chunks.size()) <=
         properties.at(TNAProperty::MAX_PACKET_BYTES_IN_CONDITION);
}

static const bdd::Node *
get_last_parser_state_op(const EP *ep, std::optional<bool> &direction) {
  const EPLeaf *leaf = ep->get_active_leaf();

  const EPNode *node = leaf->node;
  const EPNode *next = nullptr;

  while (node) {
    const Module *module = node->get_module();
    assert(module && "Module not found");

    if (module->get_type() == ModuleType::Tofino_IfHeaderValid) {
      assert(next && next->get_module());
      const Module *next_module = next->get_module();

      if (next_module->get_type() == ModuleType::Tofino_Then) {
        direction = true;
      } else {
        direction = false;
      }
    }

    if (module->get_type() == ModuleType::Tofino_ParseHeader ||
        module->get_type() == ModuleType::Tofino_IfHeaderValid) {
      return module->get_node();
    }

    next = node;
    node = node->get_prev();
  }

  return nullptr;
}

void TNA::update_parser_condition(const EP *ep,
                                  klee::ref<klee::Expr> condition) {
  const EPLeaf *leaf = ep->get_active_leaf();
  const bdd::Node *current_op = leaf->next;
  bdd::node_id_t id = current_op->get_id();

  std::optional<bool> direction;
  const bdd::Node *last_op = get_last_parser_state_op(ep, direction);

  if (!last_op) {
    // No leaf node found, add the initial parser state.
    parser.add_condition(id, condition);
    return;
  }

  bdd::node_id_t leaf_id = last_op->get_id();
  parser.add_condition(leaf_id, id, condition, direction);
}

void TNA::update_parser_transition(const EP *ep, klee::ref<klee::Expr> hdr) {
  const EPLeaf *leaf = ep->get_active_leaf();
  const bdd::Node *current_op = leaf->next;
  bdd::node_id_t id = current_op->get_id();

  std::optional<bool> direction;
  const bdd::Node *last_op = get_last_parser_state_op(ep, direction);

  if (!last_op) {
    // No leaf node found, add the initial parser state.
    parser.add_extractor(id, hdr);
    return;
  }

  bdd::node_id_t leaf_id = last_op->get_id();
  parser.add_extractor(leaf_id, id, hdr, direction);
}

void TNA::update_parser_accept(const EP *ep) {
  const EPLeaf *leaf = ep->get_active_leaf();
  const bdd::Node *current_op = leaf->next;
  bdd::node_id_t id = current_op->get_id();

  std::optional<bool> direction;
  const bdd::Node *last_op = get_last_parser_state_op(ep, direction);
  assert(last_op && "Last borrow node not found");

  bdd::node_id_t leaf_id = last_op->get_id();
  parser.accept(leaf_id, id, direction);
}

void TNA::update_parser_reject(const EP *ep) {
  const EPLeaf *leaf = ep->get_active_leaf();
  const bdd::Node *current_op = leaf->next;
  bdd::node_id_t id = current_op->get_id();

  std::optional<bool> direction;
  const bdd::Node *last_op = get_last_parser_state_op(ep, direction);
  assert(last_op && "Last borrow node not found");

  bdd::node_id_t leaf_id = last_op->get_id();
  parser.reject(leaf_id, id, direction);
}

} // namespace tofino
} // namespace synapse