#include "register.h"

namespace synapse {
namespace tofino {

Register::Register(DS_ID _id, int _num_entries, int _num_actions,
                   int _index_size, klee::ref<klee::Expr> _value)
    : DS(DSType::REGISTER, _id), num_entries(_num_entries),
      num_actions(_num_actions), index_size(_index_size), value(_value) {}

Register::Register(const Register &other)
    : DS(DSType::REGISTER, other.id), num_entries(other.num_entries),
      num_actions(other.num_actions), index_size(other.index_size),
      value(other.value) {}

DS *Register::clone() const { return new Register(*this); }

bits_t Register::get_consumed_sram() const {
  return value->getWidth() * num_entries;
}

void Register::log_debug() const {
  Log::dbg() << "\n";
  Log::dbg() << "========== REGISTER ==========\n";
  Log::dbg() << "ID:      " << id << "\n";
  Log::dbg() << "Entries: " << num_entries << "\n";
  Log::dbg() << "Actions: " << num_actions << "\n";
  Log::dbg() << "Index:   " << index_size << "\n";
  Log::dbg() << "SRAM:    " << get_consumed_sram() / 8 << " B\n";
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