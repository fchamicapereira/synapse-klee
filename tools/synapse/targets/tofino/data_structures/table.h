#pragma once

#include "data_structure.h"

#include <vector>

namespace synapse {
namespace tofino {

struct Table : public DataStructure {
  int num_entries;
  std::vector<klee::ref<klee::Expr>> keys;
  std::vector<klee::ref<klee::Expr>> params;
  symbol_t hit;

  Table(DataStructureID _id, int _num_entries,
        const std::vector<klee::ref<klee::Expr>> &_keys,
        const std::vector<klee::ref<klee::Expr>> &_params, symbol_t _hit)
      : DataStructure(DSType::SIMPLE_TABLE, _id), num_entries(_num_entries),
        keys(_keys), params(_params), hit(_hit) {}

  Table(const Table &other)
      : DataStructure(DSType::SIMPLE_TABLE, other.id),
        num_entries(other.num_entries), keys(other.keys), params(other.params),
        hit(other.hit) {}

  DataStructure *clone() const override { return new Table(*this); }
};

} // namespace tofino
} // namespace synapse