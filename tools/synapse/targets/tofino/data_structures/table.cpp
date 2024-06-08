#include "table.h"

namespace synapse {
namespace tofino {

Table::Table(DS_ID _id, int _num_entries, const std::vector<bits_t> &_keys,
             const std::vector<bits_t> &_params)
    : DS(DSType::TABLE, _id), num_entries(_num_entries), keys(_keys),
      params(_params) {}

Table::Table(const Table &other)
    : DS(DSType::TABLE, other.id), num_entries(other.num_entries),
      keys(other.keys), params(other.params) {}

DS *Table::clone() const { return new Table(*this); }

bits_t Table::get_match_xbar_consume() const {
  bits_t consumed = 0;
  for (bits_t key_size : keys)
    consumed += key_size;
  return consumed;
}

bits_t Table::get_consumed_sram() const {
  bits_t consumed = 0;
  for (bits_t key_size : keys)
    consumed += key_size;
  for (bits_t param_size : params)
    consumed += param_size;
  return consumed * num_entries;
}

void Table::log_debug() const {
  Log::dbg() << "\n";
  Log::dbg() << "=========== TABLE ============\n";
  Log::dbg() << "ID:      " << id << "\n";
  Log::dbg() << "Entries: " << num_entries << "\n";
  Log::dbg() << "Xbar:    " << get_match_xbar_consume() / 8 << " B\n";
  Log::dbg() << "SRAM:    " << get_consumed_sram() / 8 << " B\n";
  Log::dbg() << "==============================\n";
}

std::vector<klee::ref<klee::Expr>>
Table::build_keys(klee::ref<klee::Expr> key) {
  std::vector<klee::ref<klee::Expr>> keys;

  std::vector<kutil::expr_group_t> groups = kutil::get_expr_groups(key);
  for (const kutil::expr_group_t &group : groups) {
    keys.push_back(group.expr);
  }

  return keys;
}

} // namespace tofino
} // namespace synapse