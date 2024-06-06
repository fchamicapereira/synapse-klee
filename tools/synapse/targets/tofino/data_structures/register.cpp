#include "register.h"
#include "../tna/tna.h"

namespace synapse {
namespace tofino {

Register::Register(const TNAConstraints &constraints, DS_ID _id,
                   int _num_entries, int _num_actions, int _index_size,
                   klee::ref<klee::Expr> _value)
    : DS(DSType::REGISTER, _id), num_entries(_num_entries),
      num_actions(_num_actions), index_size(_index_size), value(_value) {
  assert(value->getWidth() <= constraints.max_salu_size);
}

Register::Register(const Register &other)
    : DS(DSType::REGISTER, other.id), num_entries(other.num_entries),
      num_actions(other.num_actions), index_size(other.index_size),
      value(other.value) {}

DS *Register::clone() const { return new Register(*this); }

bits_t Register::get_consumed_sram() const {
  bits_t index_sram_block = 1024 * 128;
  return value->getWidth() * num_entries + index_sram_block;
}

void Register::log_debug() const {
  Log::dbg() << "\n";
  Log::dbg() << "========== REGISTER ==========\n";
  Log::dbg() << "ID:       " << id << "\n";
  Log::dbg() << "Entries:  " << num_entries << "\n";
  Log::dbg() << "Actions:  " << num_actions << "\n";
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