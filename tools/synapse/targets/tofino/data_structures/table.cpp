#include "table.h"

namespace synapse {
namespace tofino {

Table::Table(DS_ID _id, int _num_entries,
             const std::vector<klee::ref<klee::Expr>> &_keys,
             const std::vector<klee::ref<klee::Expr>> &_params,
             const std::optional<symbol_t> &_hit)
    : DS(DSType::SIMPLE_TABLE, _id), num_entries(_num_entries), keys(_keys),
      params(_params), hit(_hit) {}

Table::Table(const Table &other)
    : DS(DSType::SIMPLE_TABLE, other.id), num_entries(other.num_entries),
      keys(other.keys), params(other.params), hit(other.hit) {}

DS *Table::clone() const { return new Table(*this); }

bits_t Table::get_match_xbar_consume() const {
  bits_t consumed = 0;
  for (klee::ref<klee::Expr> key : keys)
    consumed += key->getWidth();
  return consumed;
}

bits_t Table::get_consumed_sram() const {
  return predict_sram_consume(num_entries, keys, params);
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

bits_t
Table::predict_sram_consume(size_t num_entries,
                            const std::vector<klee::ref<klee::Expr>> &keys,
                            const std::vector<klee::ref<klee::Expr>> &params) {
  bits_t consumed = 0;
  for (klee::ref<klee::Expr> key : keys)
    consumed += key->getWidth();
  for (klee::ref<klee::Expr> param : params)
    consumed += param->getWidth();
  return consumed * num_entries;
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