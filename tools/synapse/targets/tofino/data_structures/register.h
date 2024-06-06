#pragma once

#include "data_structure.h"
#include "../../../log.h"

#include <vector>
#include <optional>

namespace synapse {
namespace tofino {

struct TNAConstraints;

enum class RegisterAction {
  READ,  // No modification
  WRITE, // Overwrites the current value
  SWAP,  // Returns the old value
};

struct Register : public DS {
  int num_entries;
  bits_t index_size;
  klee::ref<klee::Expr> value;
  std::unordered_set<RegisterAction> actions;

  Register(const TNAConstraints &constraints, DS_ID id, int num_entries,
           int index_size, klee::ref<klee::Expr> value,
           const std::unordered_set<RegisterAction> &actions);

  Register(const Register &other);

  DS *clone() const override;
  void log_debug() const override;

  bits_t get_consumed_sram() const;
  int get_num_logical_ids() const;

  static std::vector<klee::ref<klee::Expr>>
  build_keys(klee::ref<klee::Expr> key);
};

} // namespace tofino
} // namespace synapse
