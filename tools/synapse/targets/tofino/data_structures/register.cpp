#include "register.h"
#include "../tna/tna.h"

namespace synapse {
namespace tofino {

Register::Register(const TNAConstraints &constraints, DS_ID _id,
                   int _num_entries, int _index_size,
                   klee::ref<klee::Expr> _value,
                   const std::unordered_set<RegisterAction> &_actions)
    : DS(DSType::REGISTER, _id), num_entries(_num_entries),
      index_size(_index_size), value(_value), actions(_actions) {
  assert(value->getWidth() <= constraints.max_salu_size);
}

Register::Register(const Register &other)
    : DS(DSType::REGISTER, other.id), num_entries(other.num_entries),
      index_size(other.index_size), value(other.value), actions(other.actions) {
}

DS *Register::clone() const { return new Register(*this); }

bits_t Register::get_consumed_sram() const {
  bits_t index_sram_block = 1024 * 128;
  return value->getWidth() * num_entries + index_sram_block;
}

int Register::get_num_logical_ids() const { return (int)actions.size(); }

void Register::log_debug() const {
  Log::dbg() << "\n";
  Log::dbg() << "========== REGISTER ==========\n";
  Log::dbg() << "ID:       " << id << "\n";
  Log::dbg() << "Entries:  " << num_entries << "\n";

  std::stringstream ss;

  ss << "Actions:  [";
  bool first = true;
  for (RegisterAction action : actions) {
    if (!first) {
      ss << ", ";
    }
    switch (action) {
    case RegisterAction::READ:
      ss << "READ";
      break;
    case RegisterAction::WRITE:
      ss << "WRITE";
      break;
    case RegisterAction::SWAP:
      ss << "SWAP";
      break;
    }
    first = false;
  }
  ss << "]\n";

  Log::dbg() << ss.str();
  Log::dbg() << "Index sz: " << index_size << "b\n";
  Log::dbg() << "Value sz: " << value->getWidth() << "b\n";
  Log::dbg() << "SRAM:     " << get_consumed_sram() / 8 << " B\n";
  Log::dbg() << "==============================\n";
}

std::vector<klee::ref<klee::Expr>>
Register::build_keys(klee::ref<klee::Expr> key) {
  std::vector<klee::ref<klee::Expr>> keys;

  std::vector<kutil::expr_group_t> groups = kutil::get_expr_groups(key);
  for (const kutil::expr_group_t &group : groups) {
    keys.push_back(group.expr);
  }

  return keys;
}

} // namespace tofino
} // namespace synapse