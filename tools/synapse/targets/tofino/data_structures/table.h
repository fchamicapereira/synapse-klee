#pragma once

#include "data_structure.h"
#include "../../../log.h"

#include <vector>
#include <optional>

namespace synapse {
namespace tofino {

struct Table : public DS {
  int num_entries;
  std::vector<klee::ref<klee::Expr>> keys;
  std::vector<klee::ref<klee::Expr>> params;
  std::optional<symbol_t> hit;

  Table(DS_ID id, int num_entries,
        const std::vector<klee::ref<klee::Expr>> &keys,
        const std::vector<klee::ref<klee::Expr>> &params,
        const std::optional<symbol_t> &hit);

  Table(const Table &other);

  DS *clone() const override;

  bits_t get_match_xbar_consume() const;
  bits_t get_consumed_sram() const;

  void log_debug() const;

  static std::vector<klee::ref<klee::Expr>>
  build_keys(klee::ref<klee::Expr> key);
};

} // namespace tofino
} // namespace synapse
