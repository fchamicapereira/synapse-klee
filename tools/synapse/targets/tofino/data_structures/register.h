#pragma once

#include "data_structure.h"
#include "../../../log.h"

#include <vector>
#include <optional>

namespace synapse {
namespace tofino {

struct Register : public DS {
  int num_entries;
  int num_actions;
  bits_t index_size;
  klee::ref<klee::Expr> value;

  Register(DS_ID id, int num_entries, int num_actions, int index_size,
           klee::ref<klee::Expr> value);
  Register(const Register &other);

  DS *clone() const override;

  bits_t get_consumed_sram() const;

  void log_debug() const;

  static std::vector<klee::ref<klee::Expr>>
  build_keys(klee::ref<klee::Expr> key);
};

} // namespace tofino
} // namespace synapse
