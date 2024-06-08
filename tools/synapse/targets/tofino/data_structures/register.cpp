#include "register.h"
#include "../tna/tna.h"

namespace synapse {
namespace tofino {

Register::Register(const TNAConstraints &constraints, DS_ID _id,
                   int _num_entries, bits_t _index, bits_t _value,
                   const std::unordered_set<RegisterAction> &_actions)
    : DS(DSType::REGISTER, _id), num_entries(_num_entries), index(_index),
      value(_value), actions(_actions) {
  assert(value <= constraints.max_salu_size);
}

Register::Register(const Register &other)
    : DS(DSType::REGISTER, other.id), num_entries(other.num_entries),
      index(other.index), value(other.value), actions(other.actions) {}

DS *Register::clone() const { return new Register(*this); }

bits_t Register::get_consumed_sram() const {
  bits_t index_sram_block = 1024 * 128;
  return value * num_entries + index_sram_block;
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
  Log::dbg() << "Index sz: " << index << "b\n";
  Log::dbg() << "Value sz: " << value << "b\n";
  Log::dbg() << "SRAM:     " << get_consumed_sram() / 8 << " B\n";
  Log::dbg() << "==============================\n";
}

std::vector<klee::ref<klee::Expr>>
Register::partition_value(const TNAConstraints &tna_constr,
                          klee::ref<klee::Expr> value) {
  std::vector<klee::ref<klee::Expr>> partitions;

  bits_t value_width = value->getWidth();
  bits_t partition_width = tna_constr.max_salu_size;

  bits_t offset = 0;
  while (offset < value_width) {
    if (offset + partition_width > value_width) {
      partition_width = value_width - offset;
    }

    klee::ref<klee::Expr> partition =
        kutil::solver_toolbox.exprBuilder->Extract(value, offset,
                                                   partition_width);
    partitions.push_back(partition);

    offset += partition_width;
  }

  return partitions;
}

} // namespace tofino
} // namespace synapse